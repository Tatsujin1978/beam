// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "../node.h"
#include "../../mnemonic/mnemonic.h"
#include "../../utility/cli/options.h"
#include "../../core/fly_client.h"
#include "../../core/treasury.h"
#include "../../core/serialization_adapters.h"
#include <boost/core/ignore_unused.hpp>

#define LOG_VERBOSE_ENABLED 0
#include "utility/logger.h"

namespace beam {

bool ReadSeed(Key::IKdf::Ptr& pKdf, const char* szSeed)
{
    std::vector<std::string> vWords;
    while (true)
    {
        const char* p = strchr(szSeed, ';');
        if (!p)
            break;
        if (p > szSeed)
            vWords.push_back(std::string(szSeed, p));

        szSeed = p + 1;
    };

    if (vWords.size() != beam::WORD_COUNT)
        return false;

    beam::ByteBuffer buf = beam::decodeMnemonic(vWords);

    ECC::Hash::Value hvSeed;
    ECC::Hash::Processor() << beam::Blob(buf) >> hvSeed;

    pKdf = std::make_shared<ECC::HKdf>();
    ECC::HKdf& hkdf = Cast::Up<ECC::HKdf>(*pKdf);

    hkdf.Generate(hvSeed);
    return true;
}

struct Context
{
    Key::IKdf::Ptr m_pKdf;
    ShieldedTxo::Viewer m_ShieldedViewer;

    template <typename TID, typename TBase>
    struct Txo
        :public TBase
    {
        struct ID
            :public boost::intrusive::set_base_hook<>
        {
            TID m_Value;
            bool operator < (const ID& x) const { return m_Value < x.m_Value; }

            IMPLEMENT_GET_PARENT_OBJ(Txo, m_ID)
        } m_ID;

        struct HeightNode
            :public boost::intrusive::set_base_hook<>
        {
            Height m_Confirmed;
            Height m_Spent;

            Height get() const { return std::min(m_Confirmed, m_Spent); }
            bool operator < (const HeightNode& x) const { return get() < x.get(); }

            IMPLEMENT_GET_PARENT_OBJ(Txo, m_Height)
        } m_Height;

        bool IsSpent() const
        {
            return MaxHeight != m_Height.m_Spent;
        }

        Height m_LockedUntil = 0;

        typedef boost::intrusive::multiset<HeightNode> HeightMap;
        typedef boost::intrusive::multiset<ID> IDMap;

        typedef std::multiset<TID> IDSet;

        struct Container
        {
            IDMap m_mapID;
            HeightMap m_mapConfirmed;
            HeightMap m_mapSpent;

            Txo* CreateConfirmed(const TID& tid, Height h)
            {
                Txo* pTxo = new Txo;
                pTxo->m_ID.m_Value = tid;
                m_mapID.insert(pTxo->m_ID);

                pTxo->m_Height.m_Spent = MaxHeight;

                assert(MaxHeight != h);
                pTxo->m_Height.m_Confirmed = h;
                m_mapConfirmed.insert(pTxo->m_Height);

                return pTxo;
            }

            void SetSpent(Txo& txo, Height h)
            {
                assert(MaxHeight != h);
                assert(!txo.IsSpent());
                m_mapConfirmed.erase(HeightMap::s_iterator_to(txo.m_Height));
                txo.m_Height.m_Spent = h;
                m_mapSpent.insert(txo.m_Height);
            }

            void Delete(Txo& txo)
            {
                if (txo.IsSpent())
                    m_mapSpent.erase(HeightMap::s_iterator_to(txo.m_Height));
                else
                    m_mapConfirmed.erase(HeightMap::s_iterator_to(txo.m_Height));

                m_mapID.erase(IDMap::s_iterator_to(txo.m_ID));

                delete& txo;
            }

            void OnRolledBack(Height h)
            {
                while (!m_mapSpent.empty())
                {
                    Txo& txo = m_mapSpent.rbegin()->get_ParentObj();
                    assert(txo.IsSpent());

                    if (txo.m_Height.m_Spent <= h)
                        break;

                    if (txo.m_Height.m_Confirmed > h)
                        Delete(txo);
                    else
                    {
                        txo.m_Height.m_Spent = MaxHeight;
                        m_mapSpent.erase(HeightMap::s_iterator_to(txo.m_Height));
                        m_mapConfirmed.insert(txo.m_Height);
                    }
                }
            }

            ~Container()
            {
                Clear();
            }

            void Clear()
            {
                while (!m_mapID.empty())
                    Delete(m_mapID.begin()->get_ParentObj());
            }

            void ForgetOld(Height h)
            {
                while (!m_mapSpent.empty())
                {
                    Txo& txo = m_mapSpent.begin()->get_ParentObj();
                    assert(txo.IsSpent());

                    if (txo.m_Height.m_Spent > h)
                        break;

                    Delete(txo);
                }
            }

            Txo* Find(const TID& cid)
            {
                ID id;
                id.m_Value = cid;
                typename IDMap::iterator it = m_mapID.find(id);

                return (m_mapID.end() == it) ? nullptr : &it->get_ParentObj();
            }

            uint32_t HandleTxs(IDSet& s, Height h)
            {
                uint32_t ret = 0;
                for (typename IDSet::iterator it = s.begin(); s.end() != it; )
                {
                    IDSet::iterator itThis = it++;

                    Txo* pTxo = Find(*itThis);
                    if (pTxo)
                    {
                        if (pTxo->IsSpent())
                            ret++; // tx confirmed!
                        else
                        {
                            if (pTxo->m_LockedUntil >= h)
                                continue;
                        }
                    }

                    s.erase(itThis);
                }

                return ret;
            }

        };
    };

    struct TxoBase {
        Height m_Maturity;
    };

    typedef Txo<CoinID, TxoBase> TxoMW;

    struct TxoShieldedBase
    {
        Amount m_Value;
        Asset::ID m_AssetID;
        ECC::Scalar m_kSerG;
        bool m_IsCreatedByViewer;
        ShieldedTxo::User m_User;
    };

    typedef Txo<TxoID, TxoShieldedBase> TxoSH;

    TxoMW::Container m_TxosMW;
    TxoSH::Container m_TxosSH;

    struct EventsHandler
        :public proto::FlyClient::Request::IHandler
    {
        Height m_hEvents = 0;
        proto::FlyClient::Request::Ptr m_pReq;

        virtual void OnComplete(proto::FlyClient::Request& r_) override
        {
            assert(&r_ == m_pReq.get());
            m_pReq.reset();

            proto::FlyClient::RequestEvents& r = Cast::Up<proto::FlyClient::RequestEvents>(r_);

            struct MyParser :public proto::Event::IGroupParser
            {
                Context& m_This;
                MyParser(Context& x) :m_This(x) {}

                virtual void OnEvent(proto::Event::Base& evt) override
                {
                    if (MaxHeight == m_Height)
                        return; // it shouldn't be anyway!

                    if (proto::Event::Type::Utxo == evt.get_Type())
                        return OnEventType(Cast::Up<proto::Event::Utxo>(evt));
                    if (proto::Event::Type::Shielded == evt.get_Type())
                        return OnEventType(Cast::Up<proto::Event::Shielded>(evt));
                    if (proto::Event::Type::AssetCtl == evt.get_Type())
                        return OnEventType(Cast::Up<proto::Event::AssetCtl>(evt));
                }

                void OnEventType(proto::Event::Utxo& evt)
                {
                    TxoMW::ID cid;
                    cid.m_Value = evt.m_Cid;

                    TxoMW::IDMap::iterator it = m_This.m_TxosMW.m_mapID.find(cid);

                    if (proto::Event::Flags::Add & evt.m_Flags)
                    {
                        if (m_This.m_TxosMW.m_mapID.end() == it)
                        {
                            TxoMW* pTxo = m_This.m_TxosMW.CreateConfirmed(evt.m_Cid, m_Height);
                            pTxo->m_Maturity = evt.m_Maturity;
                        }
                    }
                    else
                    {
                        if (m_This.m_TxosMW.m_mapID.end() != it)
                        {
                            TxoMW& txo = it->get_ParentObj();
                            if (!txo.IsSpent())
                                m_This.m_TxosMW.SetSpent(txo, m_Height);
                                // update txs?
                        }
                    }
                }

                void OnEventType(proto::Event::Shielded& evt)
                {
                    TxoSH::ID cid;
                    cid.m_Value = evt.m_ID;

                    TxoSH::IDMap::iterator it = m_This.m_TxosSH.m_mapID.find(cid);

                    if (proto::Event::Flags::Add & evt.m_Flags)
                    {
                        if (m_This.m_TxosSH.m_mapID.end() == it)
                        {
                            TxoSH* pTxo = m_This.m_TxosSH.CreateConfirmed(evt.m_ID, m_Height);
                            pTxo->m_Value = evt.m_Value;
                            pTxo->m_AssetID = evt.m_AssetID;
                            pTxo->m_kSerG = evt.m_kSerG;
                            pTxo->m_IsCreatedByViewer = (proto::Event::Flags::CreatedByViewer & evt.m_Flags) != 0;
                            pTxo->m_User = evt.m_User;
                        }
                    }
                    else
                    {
                        if (m_This.m_TxosSH.m_mapID.end() != it)
                        {
                            TxoSH& txo = it->get_ParentObj();
                            if (!txo.IsSpent())
                                m_This.m_TxosSH.SetSpent(txo, m_Height);
                            // update txs?
                        }
                    }
                }

                void OnEventType(proto::Event::AssetCtl& evt)
                {
                }

            } p(get_ParentObj());

            uint32_t nCount = p.Proceed(r.m_Res.m_Events);

            Height hTip = get_ParentObj().m_FlyClient.get_Height();

            if (nCount < proto::Event::s_Max)
                m_hEvents = hTip + 1;
            else
            {
                m_hEvents = p.m_Height + 1;
                if (m_hEvents < hTip + 1)
                    AskEventsStrict();
            }

            if (!m_pReq)
                get_ParentObj().OnEventsHandled();
        }

        void Abort()
        {
            if (m_pReq) {
                m_pReq->m_pTrg = nullptr;
                m_pReq.reset();
            }
        }

        ~EventsHandler() {
            Abort();
        }

        void AskEventsStrict()
        {
            assert(!m_pReq);

            m_pReq = new proto::FlyClient::RequestEvents;
            Cast::Up<proto::FlyClient::RequestEvents>(*m_pReq).m_Msg.m_HeightMin = m_hEvents;

            get_ParentObj().m_Network.PostRequest(*m_pReq, *this);
        }

        IMPLEMENT_GET_PARENT_OBJ(Context, m_EventsHandler)

    } m_EventsHandler;

    struct DummyHandler
        :public proto::FlyClient::Request::IHandler
    {
        virtual void OnComplete(proto::FlyClient::Request& r_) override
        {
        };

    } m_DummyHandler;

	struct MyFlyClient
		:public proto::FlyClient
	{

		Block::SystemState::HistoryMap m_Hist;

        Height get_Height() const {
            return m_Hist.m_Map.empty() ? 0 : m_Hist.m_Map.rbegin()->first;
        }

        virtual void OnNewTip() override
        {
            get_ParentObj().OnNewTip();
        }

        virtual void OnRolledBack() override
        {
            get_ParentObj().OnRolledBack();
        }

        virtual void get_Kdf(Key::IKdf::Ptr& pKdf) override
        {
            pKdf = get_ParentObj().m_pKdf;
        }

        virtual void get_OwnerKdf(Key::IPKdf::Ptr& pPKdf) override
        {
            pPKdf = get_ParentObj().m_pKdf;
        }

        virtual Block::SystemState::IHistory& get_History() override
        {
            return m_Hist;
        }

      
        IMPLEMENT_GET_PARENT_OBJ(Context, m_FlyClient)

    } m_FlyClient;

    proto::FlyClient::NetworkStd m_Network;

    Context()
        :m_Network(m_FlyClient)
    {
    }

    struct Cfg
    {
        Transaction::FeeSettings m_Fees; // def

        Amount m_ShieldedValue = 100; // groth
        Amount m_BulletValue = 3000; // must be big enough to cover shielded out & in txs
        uint32_t m_BulletsMin = 10;
        uint32_t m_BulletsMax = 20;
        uint32_t m_ShieldedOutsTrg = 5;
        uint32_t m_ShieldedInsTrg = 5;

    } m_Cfg;


    void OnRolledBack()
    {
        Height h = m_FlyClient.get_Height();

        m_TxosMW.OnRolledBack(h);
        m_TxosSH.OnRolledBack(h);

        m_EventsHandler.Abort();
        std::setmin(m_EventsHandler.m_hEvents, h + 1);
    }

    void OnNewTip()
    {
        Height h = m_FlyClient.get_Height();

        if (h >= Rules::get().MaxRollback)
        {
            Height h0 = h - Rules::get().MaxRollback;
            m_TxosMW.ForgetOld(h0);
            m_TxosSH.ForgetOld(h0);
        }

        if (!m_EventsHandler.m_pReq)
            m_EventsHandler.AskEventsStrict();
    }

    bool IsBullet(const CoinID& cid) const
    {
        return
            !cid.m_AssetID &&
            (cid.m_Value == m_Cfg.m_BulletValue);
    }

    TxoMW* FindNextAvail(TxoMW::HeightMap::iterator& it, Height h)
    {
        while (true)
        {
            if (m_TxosMW.m_mapConfirmed.end() == it)
                break;

            TxoMW& txo = it->get_ParentObj();
            it++;

            if ((txo.m_Maturity <= h) && (txo.m_LockedUntil < h))
                return &txo;
        }

        return nullptr;
    }

    TxoMW* FindNextAvailBullet(TxoMW::HeightMap::iterator& it, Height h)
    {
        while (true)
        {
            TxoMW* pTxo = FindNextAvail(it, h);
            if (!pTxo)
                break;

            if (IsBullet(pTxo->m_ID.m_Value))
                return pTxo;
        }

        return nullptr;
    }

    TxoMW::IDSet m_setTxsOut;
    TxoMW::IDSet m_setSplit;

    void OnEventsHandled()
    {
        // decide w.r.t. test logic
        Height h = m_FlyClient.get_Height();
        std::cout << "H=" << h << std::endl;

        if (h < Rules::get().pForks[2].m_Height)
            return;

        m_TxosMW.HandleTxs(m_setSplit, h);

        uint32_t nDone = m_TxosMW.HandleTxs(m_setTxsOut, h);
        if (nDone)
            std::cout << "\tNew confirmed shielded outs: " << nDone << std::endl;

        nDone = m_TxosSH.HandleTxs(m_setTxsIn, h);
        if (nDone)
            std::cout << "\tNew confirmed shielded ins: " << nDone << std::endl;

        TxoMW::HeightMap::iterator itBullet = m_TxosMW.m_mapConfirmed.begin();

        nDone = 0;
        while (m_setTxsOut.size() < m_Cfg.m_ShieldedOutsTrg)
        {
            TxoMW* pTxo = FindNextAvailBullet(itBullet, h);
            if (!pTxo)
                break;

            SendShieldedOutp(*pTxo);
            m_setTxsOut.insert(pTxo->m_ID.m_Value);
            nDone++;
        }

        if (nDone)
            std::cout << "\tSent shielded outs: " << nDone << std::endl;

        std::cout << "\tPending shielded outs: " << m_setTxsOut.size() << std::endl;

        // make sure we have enough bullets
        uint32_t nFreeBullets = 0;
        while (true)
        {
            TxoMW* pTxo = FindNextAvailBullet(itBullet, h);
            if (!pTxo)
                break;

            nFreeBullets++;
        }

        std::cout << "\tBullets remaining: " << nFreeBullets << std::endl;

        if (m_setSplit.empty() && (nFreeBullets < m_Cfg.m_BulletsMin))
        {
            AddSplitTx();
            std::cout << "\tMaking more bullets..." << std::endl;
        }
    }

    Amount SelectInputs(Amount val, std::vector<TxoMW*>& v)
    {
        Height h = m_FlyClient.get_Height();
        Amount ret = 0;

        for (TxoMW::HeightMap::iterator it = m_TxosMW.m_mapConfirmed.begin(); ret < val; )
        {
            TxoMW* pTxo = FindNextAvail(it, h);
            if (!pTxo)
                break; // not enough funds

            const CoinID& cid = pTxo->m_ID.m_Value;

            if (!IsBullet(cid) && !cid.m_AssetID)
            {
                v.push_back(pTxo);
                ret += cid.m_Value;
            }
        }

        return ret;
    }

    void AddInp(TxVectors::Full& txv, ECC::Scalar::Native& kOffs, const CoinID& cid)
    {
        Input::Ptr pInp = std::make_unique<Input>();

        ECC::Scalar::Native sk;
        CoinID::Worker(cid).Create(sk, pInp->m_Commitment, *cid.get_ChildKdf(m_pKdf));

        txv.m_vInputs.push_back(std::move(pInp));
        kOffs += sk;
    }

    void AddInp(TxVectors::Full& txv, ECC::Scalar::Native& kOffs, TxoMW& txo, Height hMax)
    {
        AddInp(txv, kOffs, txo.m_ID.m_Value);
        txo.m_LockedUntil = hMax;
    }

    void AddOutp(TxVectors::Full& txv, ECC::Scalar::Native& kOffs, const CoinID& cid, Height hScheme)
    {
        Output::Ptr pOutp = std::make_unique<Output>();

        ECC::Scalar::Native sk;
        pOutp->Create(hScheme, sk, *cid.get_ChildKdf(m_pKdf), cid, *m_pKdf);

        txv.m_vOutputs.push_back(std::move(pOutp));
        sk = -sk;
        kOffs += sk;
    }

    void AddOutp(TxVectors::Full& txv, ECC::Scalar::Native& kOffs, Amount val, Asset::ID aid, Height hScheme)
    {
        CoinID cid(Zero);
        ECC::GenRandom(&cid.m_Idx, sizeof(cid.m_Idx));
        cid.m_Type = Key::Type::Regular;
        cid.m_Value = val;
        cid.m_AssetID = aid;

        AddOutp(txv, kOffs, cid, hScheme);
    }

    void AddSplitTx()
    {
        Height h = m_FlyClient.get_Height();

        Amount valFee = m_Cfg.m_Fees.m_Kernel + m_Cfg.m_Fees.m_Output * (m_Cfg.m_BulletsMax + 1); // minimum fee (assuming there's a change output)
        Amount valInp = m_Cfg.m_BulletValue * m_Cfg.m_BulletsMax + valFee;

        std::vector<TxoMW*> vInps;
        Amount val = SelectInputs(valInp, vInps);
        if (val < valInp)
            return;

        Transaction::Ptr pTx = std::make_shared<Transaction>();

        HeightRange hr(h, h + 10);

        ECC::Scalar sk_;
        ECC::GenRandom(sk_.m_Value);
        ECC::Scalar::Native kOffs = sk_;

        TxKernelStd::Ptr pKrn = std::make_unique<TxKernelStd>();
        pKrn->m_Height = hr;
        pKrn->m_Fee = valFee;

        pKrn->Sign(kOffs);
        pTx->m_vKernels.push_back(std::move(pKrn));
        kOffs = -kOffs;

        for (size_t i = 0; i < vInps.size(); i++)
            AddInp(*pTx, kOffs, *vInps[i], hr.m_Max);

        for (uint32_t i = 0; i < m_Cfg.m_BulletsMax; i++)
            AddOutp(*pTx, kOffs, m_Cfg.m_BulletValue, 0, hr.m_Min);

        if (val > valInp)
            AddOutp(*pTx, kOffs, val - valInp, 0, hr.m_Min);

        pTx->m_Offset = kOffs;
        pTx->Normalize();

        SendTx(std::move(pTx));

        m_setSplit.insert(vInps.front()->m_ID.m_Value);
    }

    void SendShieldedOutp(TxoMW& txo)
    {
        Height h = m_FlyClient.get_Height();

        Transaction::Ptr pTx = std::make_shared<Transaction>();
        HeightRange hr(h, h + 10);

        TxKernelShieldedOutput::Ptr pKrn = std::make_unique<TxKernelShieldedOutput>();
        pKrn->m_Height = hr;

        pKrn->m_Fee = m_Cfg.m_Fees.m_Kernel + m_Cfg.m_Fees.m_Output + m_Cfg.m_Fees.m_ShieldedOutput;

        pKrn->UpdateMsg();
        ECC::Oracle oracle;
        oracle << pKrn->m_Msg;

        ShieldedTxo::Data::Params sdp;
        ZeroObject(sdp.m_Output.m_User);
        sdp.m_Output.m_AssetID = txo.m_ID.m_Value.m_AssetID;
        sdp.m_Output.m_Value = txo.m_ID.m_Value.m_Value - pKrn->m_Fee;

        ECC::uintBig nonce;
        ECC::GenRandom(nonce);
        sdp.Generate(pKrn->m_Txo, oracle, m_ShieldedViewer, nonce);
        pKrn->MsgToID();

        //ECC::Point::Native pt;
        //if (!pKrn->IsValid(hr.m_Min, pt))
        //    __debugbreak();

        pTx->m_vKernels.push_back(std::move(pKrn));
        ECC::Scalar::Native kOffs = -sdp.m_Output.m_k;

        AddInp(*pTx, kOffs, txo, hr.m_Max);

        pTx->m_Offset = kOffs;
        pTx->Normalize();

        SendTx(std::move(pTx));
    }

    void SendTx(Transaction::Ptr&& pTx)
    {
        //TxBase::Context::Params pars;
        //TxBase::Context ctx(pars);
        //ctx.m_Height.m_Min = m_FlyClient.get_Height() + 1;

        //if (!pTx->IsValid(ctx))
        //    __debugbreak();

        proto::FlyClient::RequestTransaction::Ptr pReq = new proto::FlyClient::RequestTransaction;
        pReq->m_Msg.m_Transaction = std::move(pTx);

        m_Network.PostRequest(*pReq, m_DummyHandler);

    }

};

uint16_t g_LocalNodePort = 16725;

void DoTest(Key::IKdf::Ptr& pKdf)
{
    Context ctx;

    if (ctx.m_Cfg.m_BulletValue < (ctx.m_Cfg.m_Fees.m_Kernel + ctx.m_Cfg.m_Fees.m_Output) * 2 + ctx.m_Cfg.m_Fees.m_ShieldedOutput + ctx.m_Cfg.m_Fees.m_ShieldedInput)
        throw std::runtime_error("Bullet/Fee settings not consistent");

    ctx.m_pKdf = pKdf;
    ctx.m_ShieldedViewer.FromOwner(*pKdf);
    ctx.m_Network.m_Cfg.m_vNodes.push_back(io::Address(INADDR_LOOPBACK, g_LocalNodePort));
    ctx.m_Network.Connect();


    io::Reactor::get_Current().run();
}


} // namespace beam


int main_Guarded(int argc, char* argv[])
{
    using namespace beam;

    io::Reactor::Ptr pReactor(io::Reactor::create());
    io::Reactor::Scope scope(*pReactor);
    io::Reactor::GracefulIntHandler gih(*pReactor);

    //auto logger = beam::Logger::create(LOG_LEVEL_INFO, LOG_LEVEL_INFO);

    Key::IKdf::Ptr pKdf;

    Node node;
    node.m_Cfg.m_sPathLocal = "node_net_sim.db";

    bool bLocalMode = true;

    if (!bLocalMode)
    {
        auto [options, visibleOptions] = createOptionsDescription(0);
        boost::ignore_unused(visibleOptions);
        options.add_options()
            (cli::SEED_PHRASE, po::value<std::string>()->default_value(""), "seed phrase");

        po::variables_map vm = getOptions(argc, argv, "node_net_sim.cfg", options, true);

        if (!ReadSeed(pKdf, vm[cli::SEED_PHRASE].as<std::string>().c_str()))
        {
            std::cout << options << std::endl;
            return 0;
        }

        std::vector<std::string> vPeers = getCfgPeers(vm);

        if (vm.count(cli::PORT))
        {
            uint16_t nPort = vm[cli::PORT].as<uint16_t>();
            if (nPort)
                g_LocalNodePort = nPort;
        }

        for (size_t i = 0; i < vPeers.size(); i++)
        {
            io::Address addr;
            if (addr.resolve(vPeers[i].c_str()))
            {
                if (!addr.port())
                    addr.port(g_LocalNodePort);

                node.m_Cfg.m_Connect.push_back(addr);
            }
        }

    }
    else
    {
        ECC::HKdf::Create(pKdf, 225U);

        PeerID pid;
        ECC::Scalar::Native sk;
        Treasury::get_ID(*pKdf, pid, sk);

        Treasury tres;
        Treasury::Parameters pars;
        pars.m_Bursts = 1;
        pars.m_MaturityStep = 1;
        Treasury::Entry* pE = tres.CreatePlan(pid, Rules::get().Emission.Value0 * 200, pars);

        pE->m_pResponse.reset(new Treasury::Response);
        uint64_t nIndex = 1;
        BEAM_VERIFY(pE->m_pResponse->Create(pE->m_Request, *pKdf, nIndex));

        Treasury::Data data;
        data.m_sCustomMsg = "test treasury";
        tres.Build(data);

        beam::Serializer ser;
        ser & data;

        ser.swap_buf(node.m_Cfg.m_Treasury);

        ECC::Hash::Processor() << Blob(node.m_Cfg.m_Treasury) >> Rules::get().TreasuryChecksum;
        Rules::get().FakePoW = true;
        Rules::get().MaxRollback = 10;
        Rules::get().Shielded.MaxOuts = 2;
        Rules::get().pForks[1].m_Height = 1;
        Rules::get().pForks[2].m_Height = 2;

        node.m_Cfg.m_TestMode.m_FakePowSolveTime_ms = 3000;
        node.m_Cfg.m_MiningThreads = 1;

        beam::DeleteFile(node.m_Cfg.m_sPathLocal.c_str());
    }

    Rules::get().UpdateChecksum();

    node.m_Cfg.m_Listen.port(g_LocalNodePort);
    node.m_Cfg.m_Listen.ip(INADDR_ANY);

    // disable dandelion (faster tx propagation)
    node.m_Cfg.m_Dandelion.m_AggregationTime_ms = 0;
    node.m_Cfg.m_Dandelion.m_FluffProbability = 0xFFFF;

    node.m_Keys.SetSingleKey(pKdf);
    node.Initialize();

    if (!bLocalMode && !node.m_PostStartSynced)
    {
        struct MyObserver :public Node::IObserver
        {
            Node& m_Node;
            MyObserver(Node& n) :m_Node(n) {}

            virtual void OnSyncProgress() override
            {
                if (m_Node.m_PostStartSynced)
                    io::Reactor::get_Current().stop();
            }
        };

        MyObserver obs(node);
        Node::IObserver* pObs = &obs;
        TemporarySwap<Node::IObserver*> tsObs(pObs, node.m_Cfg.m_Observer);

        io::Reactor::get_Current().run();

        if (!node.m_PostStartSynced)
            return 1;
    }
    else
        node.m_PostStartSynced = true;

    DoTest(pKdf);

    return 0;
}

int main(int argc, char* argv[])
{
    int ret = 0;
    try
    {
        ret = main_Guarded(argc, argv);
    }
    catch (const std::exception & e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }

    return ret;
}
