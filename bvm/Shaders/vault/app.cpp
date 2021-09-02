#include "../common.h"
#include "../app_common_impl.h"
#include "contract.h"

#define Vault_manager_create(macro)
#define Vault_manager_view(macro)
#define Vault_manager_destroy(macro) macro(ContractID, cid)
#define Vault_manager_view_accounts(macro) macro(ContractID, cid)
#define Vault_manager_view_logs(macro) macro(ContractID, cid)

#define Vault_manager_view_account(macro) \
    macro(ContractID, cid) \
    macro(PubKey, pubKey)

#define VaultRole_manager(macro) \
    macro(manager, create) \
    macro(manager, destroy) \
    macro(manager, view) \
    macro(manager, view_logs) \
    macro(manager, view_accounts) \
    macro(manager, view_account)

#define Vault_my_account_view(macro) macro(ContractID, cid)
#define Vault_my_account_get_proof(macro) \
    macro(ContractID, cid) \
    macro(AssetID, aid)

#define Vault_my_account_deposit(macro) \
    macro(ContractID, cid) \
    macro(Amount, amount) \
    macro(AssetID, aid)

#define Vault_my_account_withdraw(macro) Vault_my_account_deposit(macro)

#define Vault_my_account_move(macro) \
    macro(uint8_t, isDeposit) \
    Vault_my_account_deposit(macro)

#define VaultRole_my_account(macro) \
    macro(my_account, view) \
    macro(my_account, get_proof) \
    macro(my_account, deposit) \
    macro(my_account, withdraw)

#define VaultRoles_All(macro) \
    macro(manager) \
    macro(my_account)

BEAM_EXPORT void Method_0()
{
    // scheme
    Env::DocGroup root("");

    {   Env::DocGroup gr("roles");

#define THE_FIELD(type, name) Env::DocAddText(#name, #type);
#define THE_METHOD(role, name) { Env::DocGroup grMethod(#name);  Vault_##role##_##name(THE_FIELD) }
#define THE_ROLE(name) { Env::DocGroup grRole(#name); VaultRole_##name(THE_METHOD) }
        
        VaultRoles_All(THE_ROLE)
#undef THE_ROLE
#undef THE_METHOD
#undef THE_FIELD
    }
}

#define THE_FIELD(type, name) const type& name,
#define ON_METHOD(role, name) void On_##role##_##name(Vault_##role##_##name(THE_FIELD) int unused = 0)

void OnError(const char* sz)
{
    Env::DocAddText("error", sz);
}

typedef Env::Key_T<Vault::Key> KeyAccount;


void DumpAccounts(Env::VarReader& r)
{
    Env::DocArray gr("accounts");

    while (true)
    {
        KeyAccount key;
        Amount amount;
        
        if (!r.MoveNext_T(key, amount))
            break;

        Env::DocGroup gr("");

        Env::DocAddBlob_T("Account", key.m_KeyInContract.m_Account);
        Env::DocAddNum("AssetID", key.m_KeyInContract.m_Aid);
        Env::DocAddNum("Amount", amount);
    }
}

void DumpAccount(const PubKey& pubKey, const ContractID& cid)
{
    KeyAccount k0, k1;
    k0.m_Prefix.m_Cid = cid;
    k0.m_KeyInContract.m_Account = pubKey;
    k0.m_KeyInContract.m_Aid = 0;

    _POD_(k1) = k0;
    k1.m_KeyInContract.m_Aid = static_cast<AssetID>(-1);

    Env::VarReader r(k0, k1);
    DumpAccounts(r);
}

ON_METHOD(manager, view)
{
    EnumAndDumpContracts(Vault::s_SID);
}

ON_METHOD(manager, create)
{
    Env::GenerateKernel(nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, "create Vault contract", 0);
}

ON_METHOD(manager, destroy)
{
    Env::GenerateKernel(&cid, 1, nullptr, 0, nullptr, 0, nullptr, 0, "destroy Vault contract", 0);
}

ON_METHOD(manager, view_logs)
{
    Env::Key_T<Vault::Key> k0, k1;
    _POD_(k0.m_Prefix.m_Cid) = cid;
    _POD_(k0.m_KeyInContract).SetZero();
    _POD_(k1.m_Prefix.m_Cid) = cid;
    _POD_(k1.m_KeyInContract).SetObject(0xff);

    Env::LogReader lr(k0, k1);

    Env::DocArray gr("logs");

    while (true)
    {
        Env::Key_T<Vault::Key> key;
        Amount val;

        if (!lr.MoveNext_T(key, val))
            break;

        Env::DocGroup gr("");

        Env::DocAddNum("Height", lr.m_Pos.m_Height);
        Env::DocAddNum("Pos", lr.m_Pos.m_Pos);
        Env::DocAddBlob_T("Account", key.m_KeyInContract.m_Account);
        Env::DocAddNum("AssetID", key.m_KeyInContract.m_Aid);
        Env::DocAddNum("Amount", val);
    }
}


ON_METHOD(manager, view_accounts)
{
    Env::KeyPrefix k0, k1;
    _POD_(k0.m_Cid) = cid;
    _POD_(k1.m_Cid) = cid;
    k1.m_Tag = KeyTag::Internal + 1;

    Env::VarReader r(k0, k1); // enum all internal contract vars
    DumpAccounts(r);
}

ON_METHOD(manager, view_account)
{
    DumpAccount(pubKey, cid);
}

#pragma pack (push, 1)
struct MyAccountID
{
    ContractID m_Cid;
    uint8_t m_Ctx = 0;
};
#pragma pack (pop)

void DeriveMyPk(PubKey& pubKey, const ContractID& cid)
{
    MyAccountID myid;
    myid.m_Cid = cid;

    Env::DerivePk(pubKey, &myid, sizeof(myid));
}

ON_METHOD(my_account, move)
{
    if (!amount)
        return OnError("amount should be nnz");

    Vault::Request arg;
    arg.m_Amount = amount;
    arg.m_Aid = aid;
    DeriveMyPk(arg.m_Account, cid);

    FundsChange fc;
    fc.m_Amount = arg.m_Amount;
    fc.m_Aid = arg.m_Aid;
    fc.m_Consume = isDeposit;

    if (isDeposit)
        Env::GenerateKernel(&cid, Vault::Deposit::s_iMethod, &arg, sizeof(arg), &fc, 1, nullptr, 0, "deposit to Vault", 0);
    else
    {
        MyAccountID myid;
        myid.m_Cid = cid;

#if 0

        PubKey pkFullBlind, pkFullNonce, pkMy;
        Env::DerivePk(pkMy, &myid, sizeof(myid));

        Height hMin = Env::get_Height() + 1;
        Height hMax = hMin + 10;

        const uint32_t iSlotMy = 0;
        const uint32_t iSlotKrn = 1;
        const uint32_t iSlotBlind = 2;

        Secp_scalar* s0 = Env::Secp_Scalar_alloc();
        Secp_point* p0 = Env::Secp_Point_alloc();
        Secp_point* p1 = Env::Secp_Point_alloc();

        Env::get_SlotImage(*p0, iSlotBlind); // kernel blinding factor
        Env::Secp_Point_Export(*p0, pkFullBlind);

        Env::get_SlotImage(*p0, iSlotMy); // my nonce
        Env::get_SlotImage(*p1, iSlotKrn); // kernel nonce
        Env::Secp_Point_add(*p0, *p0, *p1); // total nonce
        Env::Secp_Point_Export(*p0, pkFullNonce);

        Secp_scalar_data pE[1]; // my challenge

        // 1st call. Show our pk and nonce image (denoted by slot). Get the challenge
        Env::GenerateKernelAdvanced(&cid, Vault::Withdraw::s_iMethod, &arg, sizeof(arg), &fc, 1, &pkMy, 1, "", 0, hMin, hMax, pkFullBlind, pkFullNonce, pE[0], iSlotBlind, iSlotKrn, pE);

        Env::Secp_Scalar_import(*s0, pE[0]);

        // get the blinded sk.
        Env::get_BlindSk(*s0, &myid, sizeof(myid), *s0, iSlotMy);

        Env::Secp_Scalar_export(*s0, pE[0]);

        // 2nd call. Same args, this time we put real sk (blinded, answering the correct challenge)
        Env::GenerateKernelAdvanced(&cid, Vault::Withdraw::s_iMethod, &arg, sizeof(arg), &fc, 1, &pkMy, 1, "withdraw from Vault", 0, hMin, hMax, pkFullBlind, pkFullNonce, pE[0], iSlotBlind, iSlotKrn, nullptr);

        Env::Secp_Scalar_free(*s0);
        Env::Secp_Point_free(*p0);
        Env::Secp_Point_free(*p1);

#else

        SigRequest sig;
        sig.m_pID = &myid;
        sig.m_nID = sizeof(myid);

        Env::GenerateKernel(&cid, Vault::Withdraw::s_iMethod, &arg, sizeof(arg), &fc, 1, &sig, 1, "withdraw from Vault", 0);

#endif
    }
}

ON_METHOD(my_account, deposit)
{
    On_my_account_move(1, cid, amount, aid);
}

ON_METHOD(my_account, withdraw)
{
    On_my_account_move(0, cid, amount, aid);
}

ON_METHOD(my_account, view)
{
    PubKey pubKey;
    DeriveMyPk(pubKey, cid);
    DumpAccount(pubKey, cid);
}

ON_METHOD(my_account, get_proof)
{
    KeyAccount key;
    _POD_(key.m_Prefix.m_Cid) = cid;
    DeriveMyPk(key.m_KeyInContract.m_Account, cid);
    key.m_KeyInContract.m_Aid = aid;

    Amount* pAmount;
    uint32_t nSizeVal;
    const Merkle::Node* pProof;
    uint32_t nProof = Env::VarGetProof(&key, sizeof(key), (const void**) &pAmount, &nSizeVal, &pProof);

    if (nProof && sizeof(*pAmount) == nSizeVal)
    {
        Env::DocAddNum("Amount", *pAmount);
        Env::DocAddBlob("proof", pProof, sizeof(*pProof) * nProof);
    }
}

#undef ON_METHOD
#undef THE_FIELD

BEAM_EXPORT void Method_1()
{
    Env::DocGroup root("");

    char szRole[0x10], szAction[0x10];

    if (!Env::DocGetText("role", szRole, sizeof(szRole)))
        return OnError("Role not specified");

    if (!Env::DocGetText("action", szAction, sizeof(szAction)))
        return OnError("Action not specified");

#define PAR_READ(type, name) type arg_##name; Env::DocGet(#name, arg_##name);
#define PAR_PASS(type, name) arg_##name,

#define THE_METHOD(role, name) \
        if (!Env::Strcmp(szAction, #name)) { \
            Vault_##role##_##name(PAR_READ) \
            On_##role##_##name(Vault_##role##_##name(PAR_PASS) 0); \
            return; \
        }

#define THE_ROLE(name) \
    if (!Env::Strcmp(szRole, #name)) { \
        VaultRole_##name(THE_METHOD) \
        return OnError("invalid Action"); \
    }

    VaultRoles_All(THE_ROLE)

#undef THE_ROLE
#undef THE_METHOD
#undef PAR_PASS
#undef PAR_READ

    OnError("unknown Role");
}

