// Copyright 2020 The Beam Team
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

#include "push_tx_builder.h"

#include "core/shielded.h"

namespace beam::wallet::lelantus
{
    PushTxBuilder::PushTxBuilder(BaseTransaction& tx)
        : BaseTxBuilder(tx, kDefaultSubTxID)
    {
        m_Value = GetParameterStrict<Amount>(TxParameterID::Amount);
        GetParameter(TxParameterID::AssetID, m_AssetID);

        if (m_pKrn)
            m_Signing = Stage::Done;
    }

    void PushTxBuilder::SignSendShielded()
    {
        if (Stage::None != m_Signing)
            return;

        struct MyHandler
            :public KeyKeeperHandler
        {
            using KeyKeeperHandler::KeyKeeperHandler;

            IPrivateKeyKeeper2::Method::SignSendShielded m_Method;

            virtual ~MyHandler() {} // auto

            virtual void OnSuccess(BaseTxBuilder& b_) override
            {
                PushTxBuilder& b = Cast::Up<PushTxBuilder>(b_);

                b.AddOffset(m_Method.m_kOffset);

                b.AddKernel(std::move(m_Method.m_pKernel));
                b.SaveKernel();
                b.SaveKernelID();

                OnAllDone(b);
            }
        };

        KeyKeeperHandler::Ptr pHandler = std::make_shared<MyHandler>(*this, m_Signing);
        MyHandler& x = Cast::Up<MyHandler>(*pHandler);
        IPrivateKeyKeeper2::Method::SignSendShielded& m = x.m_Method;

        SetCommon(m);
        ZeroObject(m.m_User);

        m.m_pVoucher = std::make_unique<ShieldedTxo::Voucher>();

        if (GetParameter(TxParameterID::PeerEndpoint, m.m_Peer))
        {
            auto pOffline = std::make_unique<IPrivateKeyKeeper2::Method::SignSendShielded::Offline>();

            if (GetParameter(TxParameterID::PublicAddreessGen, pOffline->m_Addr))
            {
                // public offline tx
                if (!GetParameter(TxParameterID::PublicAddressGenSig, pOffline->m_Signature))
                    throw TransactionFailedException(true, TxFailureReason::NoVoucher);

                ECC::GenRandom(pOffline->m_Nonce);

                m.m_pOffline = std::move(pOffline);
                m.m_pVoucher.reset();
            }
            else
            {
                if (!GetParameter(TxParameterID::Voucher, *m.m_pVoucher))
                {
                    WalletID widPeer;
                    if (!GetParameter(TxParameterID::PeerAddr, widPeer))
                        throw TransactionFailedException(true, TxFailureReason::NoVoucher);

                    boost::optional<ShieldedTxo::Voucher> res;
                    m_Tx.GetGateway().get_UniqueVoucher(widPeer, m_Tx.GetTxID(), res);

                    if (!res)
                        return;

                    *m.m_pVoucher = std::move(*res);
                    SetParameter(TxParameterID::Voucher, *m.m_pVoucher);
                }
            }

            // set sender info
            m_Tx.GetMyEndpointAlways(m.m_User.m_Sender);
        }
        else
        {
            // We're sending to ourselves. Create our voucher
            m.m_iEndpoint = m_Tx.EnsureOwnID();

            m_Tx.GetWalletDB()->get_Endpoint(m.m_Peer, m.m_iEndpoint);

            IPrivateKeyKeeper2::Method::CreateVoucherShielded m2;
            m2.m_iEndpoint = m.m_iEndpoint;
            ECC::GenRandom(m2.m_Nonce);

            if (IPrivateKeyKeeper2::Status::Success != m_Tx.get_KeyKeeperStrict()->InvokeSync(m2) || m2.m_Res.empty())
                throw TransactionFailedException(true, TxFailureReason::KeyKeeperError);

            *m.m_pVoucher = m2.m_Res.front();
        }


        auto* packedMessage = ShieldedTxo::User::ToPackedMessage(m.m_User);
        packedMessage->m_TxID = Blob(m_Tx.GetTxID().data(), static_cast<uint32_t>(m_Tx.GetTxID().size()));
        uint8_t maxPrivacyMinAnonimitySet = 0;
        if (GetParameter(TxParameterID::MaxPrivacyMinAnonimitySet, maxPrivacyMinAnonimitySet))
            packedMessage->m_MaxPrivacyMinAnonymitySet = maxPrivacyMinAnonimitySet;

        // store flags
        uint8_t flags = 0;
        bool is_selfTx = false;
        GetParameter(TxParameterID::IsSelfTx, is_selfTx);
        if (is_selfTx)
            flags = flags | kIsSelfTxBit;

        packedMessage->m_Flags = flags;

        ShieldedTxo::Viewer viewer;
        viewer.FromOwner(*m_Tx.GetWalletDB()->get_OwnerKdf(), 0);

        ShieldedTxo::DataParams pars;
        if (m.m_pVoucher && pars.m_Ticket.Recover(m.m_pVoucher->m_Ticket, viewer))
        {
            // sending to yourself
            pars.m_Output.m_User = m.m_User;
            pars.m_Output.m_Value = m_Value;
            pars.m_Output.m_AssetID = m_AssetID;


            // save shielded Coin
            ShieldedCoin shieldedCoin;
            shieldedCoin.m_createTxId = m_Tx.GetTxID();

            shieldedCoin.m_CoinID.m_Key.m_nIdx = 0;
            pars.ToID(shieldedCoin.m_CoinID);

            m_Tx.GetWalletDB()->saveShieldedCoin(shieldedCoin);
        }

        m_Tx.get_KeyKeeperStrict()->InvokeAsync(m, pHandler);

    }

    void PushTxBuilder::ResetSig()
    {
        if (!m_pKrn)
            return;

        m_pTransaction->m_vKernels.clear();
        GetParameter(TxParameterID::ExtraKernels, m_pTransaction->m_vKernels);

        m_pKrn = nullptr;
        m_Signing = Stage::None;
        SaveKernel();
        SetParameter(TxParameterID::KernelID, Zero);
        SetStatus(Status::None);

        m_pTransaction->m_Offset = Zero;
        SetParameter(TxParameterID::Offset, m_pTransaction->m_Offset);

        SetParameter(TxParameterID::Voucher, Zero);

        SetParameter(TxParameterID::KernelProofHeight, Zero);
        SetParameter(TxParameterID::KernelUnconfirmedHeight, Zero);

        SetParameter(TxParameterID::TransactionRegisteredInternal, Zero);
    }

    const ShieldedTxo* PushTxBuilder::get_Txo()
    {
        if (!m_pKrn)
            return nullptr;

        if (TxKernel::Subtype::ShieldedOutput == m_pKrn->get_Subtype())
            return &m_pKrn->CastTo_ShieldedOutput().m_Txo;

        for (const auto& p : m_pKrn->m_vNested)
        {
            if (TxKernel::Subtype::ShieldedOutput == p->get_Subtype())
                return &p->CastTo_ShieldedOutput().m_Txo;
        }

        return nullptr;
    }

    const ShieldedTxo& PushTxBuilder::get_TxoStrict()
    {
        const ShieldedTxo* pTxo = get_Txo();
        if (!pTxo)
            throw TransactionFailedException(true, TxFailureReason::FailedToGetParameter);
        return *pTxo;
    }

    void PushTxBuilder::FillUserData(Output::User::Packed* user)
    {
        BaseTxBuilder::FillUserData(user);
        user->m_Amount = m_Value;
        PeerID peerEndpoint = Zero;
        GetParameter(TxParameterID::PeerEndpoint, peerEndpoint);
        user->m_Peer = peerEndpoint;
    }

} // namespace beam::wallet::lelantus