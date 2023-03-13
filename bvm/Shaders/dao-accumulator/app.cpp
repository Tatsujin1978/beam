#include "../common.h"
#include "../common.h"
#include "../app_common_impl.h"
#include "../upgradable3/app_common_impl.h"
#include "contract.h"
#include "../dao-core/contract.h"

#define DaoAcc_schedule_upgrade(macro) Upgradable3_schedule_upgrade(macro)
#define DaoAcc_replace_admin(macro) Upgradable3_replace_admin(macro)
#define DaoAcc_set_min_approvers(macro) Upgradable3_set_min_approvers(macro)
#define DaoAcc_explicit_upgrade(macro) macro(ContractID, cid)

#define DaoAcc_deploy(macro) \
    Upgradable3_deploy(macro) \
    macro(Height, hPrePhaseEnd) \
    macro(ContractID, cidDaoCore)

#define DaoAcc_view_deployed(macro)
#define DaoAcc_view_params(macro) macro(ContractID, cid)
#define DaoAcc_user_view(macro) macro(ContractID, cid)
#define DaoAcc_users_view_all(macro) macro(ContractID, cid)

#define DaoAcc_user_lock_prephase(macro) \
    macro(ContractID, cid) \
    macro(Amount, amountBeamX) \
    macro(uint32_t, lockPeriods)

#define DaoAccActions_All(macro) \
    macro(view_deployed) \
    macro(view_params) \
    macro(deploy) \
	macro(schedule_upgrade) \
	macro(replace_admin) \
	macro(set_min_approvers) \
	macro(explicit_upgrade) \
	macro(user_view) \
	macro(users_view_all) \
	macro(user_lock_prephase) \


namespace DaoAccumulator {

BEAM_EXPORT void Method_0()
{
    // scheme
    Env::DocGroup root("");

    {   Env::DocGroup gr("actions");

#define THE_FIELD(type, name) Env::DocAddText(#name, #type);
#define THE_ACTION(name) { Env::DocGroup gr(#name);  DaoAcc_##name(THE_FIELD) }
        
        DaoAccActions_All(THE_ACTION)
#undef THE_ACTION
#undef THE_FIELD
    }
}

#define THE_FIELD(type, name) const type& name,
#define ON_METHOD(name) void On_##name(DaoAcc_##name(THE_FIELD) int unused = 0)

void OnError(const char* sz)
{
    Env::DocAddText("error", sz);
}

const char g_szAdminSeed[] = "upgr3-dao-accumulator";

struct AdminKeyID :public Env::KeyID {
    AdminKeyID() :Env::KeyID(g_szAdminSeed, sizeof(g_szAdminSeed)) {}
};

const Upgradable3::Manager::VerInfo g_VerInfo = { s_pSID, _countof(s_pSID) };

ON_METHOD(view_deployed)
{
    AdminKeyID kid;
    g_VerInfo.DumpAll(&kid);
}

AssetID ReadAidBeamX(const ContractID& cidDaoCore)
{
    Env::Key_T<uint8_t> key;
    _POD_(key.m_Prefix.m_Cid) = cidDaoCore;
    key.m_KeyInContract = DaoCore::State::s_Key;

    DaoCore::State s;
    if (!Env::VarReader::Read_T(key, s))
        s.m_Aid = 0;

    if (!s.m_Aid)
        OnError("Dao-Core state not found");

    return s.m_Aid;
}

ON_METHOD(deploy)
{
    AdminKeyID kid;
    PubKey pk;
    kid.get_Pk(pk);

    Method::Create arg;
    if (!g_VerInfo.FillDeployArgs(arg.m_Upgradable, &pk))
        return;

    if (hPrePhaseEnd <= Env::get_Height())
        return OnError("pre-phase too short");
    arg.m_hPrePhaseEnd = hPrePhaseEnd;

    arg.m_aidBeamX = ReadAidBeamX(cidDaoCore);
    if (!arg.m_aidBeamX)
        return;

    const uint32_t nCharge =
        Upgradable3::Manager::get_ChargeDeploy() +
        Env::Cost::SaveVar_For(sizeof(State)) +
        Env::Cost::Cycle * 50;

    Env::GenerateKernel(nullptr, arg.s_iMethod, &arg, sizeof(arg), nullptr, 0, nullptr, 0, "Deploy Dao-Accumulator contract", nCharge);
}

ON_METHOD(schedule_upgrade)
{
    AdminKeyID kid;
    g_VerInfo.ScheduleUpgrade(cid, kid, hTarget);
}

ON_METHOD(explicit_upgrade)
{
    Upgradable3::Manager::MultiSigRitual::Perform_ExplicitUpgrade(cid);
}

ON_METHOD(replace_admin)
{
    AdminKeyID kid;
    Upgradable3::Manager::MultiSigRitual::Perform_ReplaceAdmin(cid, kid, iAdmin, pk);
}

ON_METHOD(set_min_approvers)
{
    AdminKeyID kid;
    Upgradable3::Manager::MultiSigRitual::Perform_SetApprovers(cid, kid, newVal);
}

struct MyState
    :public State
{
    bool Read(const ContractID& cid)
    {
        Env::Key_T<uint8_t> key;
        _POD_(key.m_Prefix.m_Cid) = cid;
        key.m_KeyInContract = Tags::s_State;

        if (Env::VarReader::Read_T(key, *this))
            return true;

        OnError("State not found");
        return false;
    }
};

ON_METHOD(view_params)
{
    MyState s;
    if (!s.Read(cid))
        return;

    Env::DocGroup gr("res");
    Env::DocAddNum("aid-beamX", s.m_aidBeamX);
    Env::DocAddNum("hPreEnd", s.m_hPreEnd);

    Amount totalBeamX = WalkerFunds::FromContract_Lo(cid, s.m_aidBeamX);

    if (s.m_aidLpToken)
    {
        Env::DocAddNum("aid-LpToken", s.m_aidLpToken);
        Env::DocAddNum("locked-LpToken", WalkerFunds::FromContract_Lo(cid, s.m_aidLpToken));

        s.m_Pool.Update(Env::get_Height());

        if (s.m_Pool.m_hRemaining)
        {
            Env::DocAddNum("farm-remaining-height", s.m_Pool.m_hRemaining);
            Env::DocAddNum("farm-remaining-beamX", s.m_Pool.m_AmountRemaining);
        }

        Env::DocAddNum("farm-beamX-claimable", totalBeamX - s.m_Pool.m_AmountRemaining);
    }
    else
    {
        Env::DocAddNum("locked-Beam", WalkerFunds::FromContract_Lo(cid, 0));
        Env::DocAddNum("locked-BeamX", totalBeamX);
    }
}

struct MyUser
    :public User
{
    bool Load(const ContractID& cid)
    {
        Env::Key_T<User::Key> uk;
        _POD_(uk.m_Prefix.m_Cid) = cid;
        Env::KeyID(cid).get_Pk(uk.m_KeyInContract.m_pk);

        if (Env::VarReader::Read_T(uk, *this))
            return true;

        OnError("no user");
        return false;
    }

    void Print(State& s)
    {
        Env::DocAddNum32("lock-periods", m_PrePhaseLockPeriods);
        Env::DocAddNum("lpToken-pre", m_LpTokenPrePhase);

        Env::DocAddNum("unlock-height", get_UnlockHeight(s));

        if (s.m_aidBeamX)
        {
            Env::DocAddNum("lpToken-post", m_LpTokenPostPhase);

            m_EarnedBeamX += s.m_Pool.Remove(m_PoolUser);
            Env::DocAddNum("avail-BeamX", m_EarnedBeamX);
        }

    }
};

ON_METHOD(user_view)
{
    MyState s;
    if (!s.Read(cid))
        return;

    if (s.m_aidBeamX)
        s.m_Pool.Update(Env::get_Height());

    MyUser u;
    if (!u.Load(cid))
        return;

    Env::DocGroup gr("res");

    u.Print(s);
}

ON_METHOD(users_view_all)
{
    MyState s;
    if (!s.Read(cid))
        return;

    if (s.m_aidBeamX)
        s.m_Pool.Update(Env::get_Height());

    Env::Key_T<User::Key> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k1.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract.m_pk).SetZero();
    _POD_(k1.m_KeyInContract.m_pk).SetObject(0xff);

    Env::DocArray gr0("res");

    MyUser u;
    for (Env::VarReader r(k0, k1); r.MoveNext_T(k0, u); )
    {
        Env::DocGroup gr1("");
        u.Print(s);
    }
}

ON_METHOD(user_lock_prephase)
{
    if (!amountBeamX)
        return OnError("amount not specified");
    if (lockPeriods > User::s_PreLockPeriodsMax)
        return OnError("lock period too large");

    MyState s;
    if (!s.Read(cid))
        return;

    if (s.m_hPreEnd >= Env::get_Height())
        return OnError("pre-phase is over");

    Method::UserLockPrePhase arg;
    arg.m_AmountBeamX = amountBeamX;
    arg.m_PrePhaseLockPeriods = lockPeriods;
    Env::KeyID(cid).get_Pk(arg.m_pkUser);

    FundsChange pFc[2];
    pFc[0].m_Aid = 0;
    pFc[0].m_Consume = 1;
    pFc[0].m_Amount = amountBeamX * State::s_InitialRatio;
    pFc[1].m_Aid = s.m_aidBeamX;
    pFc[1].m_Consume = 1;
    pFc[1].m_Amount = amountBeamX;

    const uint32_t nCharge =
        Env::Cost::CallFar +
        Env::Cost::LoadVar_For(sizeof(State)) +
        Env::Cost::SaveVar_For(sizeof(State)) +
        Env::Cost::LoadVar_For(sizeof(User)) +
        Env::Cost::SaveVar_For(sizeof(User)) +
        Env::Cost::FundsLock * 2 +
        Env::Cost::Cycle * 200;

    Env::GenerateKernel(&cid, arg.s_iMethod, &arg, sizeof(arg), pFc, _countof(pFc), nullptr, 0, "Dao-Accumulator Lock pre-phase", nCharge);
}

#undef ON_METHOD
#undef THE_FIELD

BEAM_EXPORT void Method_1() 
{
    Env::DocGroup root("");

    char szAction[0x20];

    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("Action not specified");

#define PAR_READ(type, name) type arg_##name; Env::DocGet(#name, arg_##name);
#define PAR_PASS(type, name) arg_##name,

#define THE_METHOD(name) \
        static_assert(sizeof(szAction) >= sizeof(#name)); \
        if (!Env::Strcmp(szAction, #name)) { \
            DaoAcc_##name(PAR_READ) \
            On_##name(DaoAcc_##name(PAR_PASS) 0); \
            return; \
        }

    DaoAccActions_All(THE_METHOD)

#undef THE_METHOD
#undef PAR_PASS
#undef PAR_READ

    OnError("unknown action");
}

} // namespace DaoAccumulator