#include "../common.h"
#include "../upgradable/contract.h"
#include "../vault/contract.h"
#include "../faucet/contract.h"
#include "../demoXdao/contract.h"

void get_ShaderID(ShaderID& sid, void* pBuf, uint32_t nBuf)
{
	static const char szName[] = "contract.shader";

	HashProcessor::Sha256 hp;
	hp
		<< "bvm.shader.id"
		<< nBuf;

	hp.Write(pBuf, nBuf);
	hp >> sid;
}

bool get_ShaderID(ShaderID& sid, const ContractID& cid)
{
	Env::VarReader r(sid, sid);

	uint32_t nKey = 0, nVal = 0;
	if (!r.MoveNext(nullptr, nKey, nullptr, nVal, 0))
		return false;

	void* pVal = Env::Heap_Alloc(nVal);

	nKey = 0;
	r.MoveNext(nullptr, nKey, pVal, nVal, 1);

	get_ShaderID(sid, pVal, nVal);

	Env::Heap_Free(pVal);
	return true;
}


#define HandleContractsAll(macro) \
	macro(Upgradable) \
	macro(Vault) \
	macro(Faucet)


struct ParserContext
{
	const ShaderID& m_Sid;
	const ContractID& m_Cid;
	uint32_t m_iMethod;
	const void* m_pArg;
	uint32_t m_nArg;

	bool m_Name = true;
	bool m_Method = false;
	bool m_State = false;

	ParserContext(const ShaderID& sid, const ContractID& cid)
		:m_Sid(sid)
		, m_Cid(cid)
	{
	}

#define THE_MACRO(name) void On_##name();
	HandleContractsAll(THE_MACRO)
#undef THE_MACRO

	bool Parse();

	void WriteUnk();
	bool WriteStdMethod();
	void WriteUpgradeParams(const Upgradable::Next&);
};

bool ParserContext::Parse()
{
#define THE_MACRO(name) \
	if (_POD_(m_Sid) == name::s_SID) \
	{ \
		if (m_Name) \
			Env::DocAddText("", #name); \
		if (m_Method) \
			Env::DocAddText("", "/"); \
		On_##name(); \
		return true; \
	}

	HandleContractsAll(THE_MACRO)
#undef THE_MACRO

	return false;
}

bool ParserContext::WriteStdMethod()
{
	assert(m_Method);
	switch (m_iMethod)
	{
	case 0: Env::DocAddText("", "Create"); break;
	case 1: Env::DocAddText("", "Destroy"); break;
	default:
		return false;
	}
	return true;
}

void ParserContext::WriteUnk()
{
	if (m_Name)
	{
		Env::DocAddText("", "sid=");
		Env::DocAddBlob_T("", m_Sid);
	}

	if (m_Method)
	{
		Env::DocAddText("", "/");
		if (!WriteStdMethod())
		{
			Env::DocAddText("", "Method=");
			Env::DocAddNum("", m_iMethod);
		}

		Env::DocAddText("", ", Args=");
		Env::DocAddBlob("", m_pArg, m_nArg);
	}
}

void ParserContext::On_Upgradable()
{
	// Get state, discover which cid actually operates the contract
	Upgradable::State us;

	if (m_Method && !m_iMethod)
	{
		// c'tor, the state doesn't exist yet. Initial cid should be in the args
		if (m_nArg < sizeof(Upgradable::Create))
			return;

		const auto& arg = *(const Upgradable::Create*) m_pArg;

		_POD_(Cast::Down<Upgradable::Current>(us)) = arg;
		_POD_(Cast::Down<Upgradable::Next>(us)).SetZero();
	}
	else
	{
		Env::Key_T<uint8_t> uk;
		_POD_(uk.m_Prefix.m_Cid) = m_Cid;
		uk.m_KeyInContract = Upgradable::State::s_Key;

		if (!Env::VarReader::Read_T(uk, us))
			return;
	}

	ShaderID sid;
	if (!get_ShaderID(sid, us.m_Cid))
		return;

	ParserContext pc2(sid, m_Cid);
	pc2.m_Name = m_Name;
	pc2.m_Method = m_Method;
	pc2.m_State = m_State;
	pc2.m_iMethod = m_iMethod;
	pc2.m_pArg = m_pArg;
	pc2.m_nArg = m_nArg;

	bool bIsUpgrade = m_Method && (Upgradable::ScheduleUpgrade::s_iMethod == m_iMethod);

	if (bIsUpgrade)
		pc2.m_Method = false;

	if (!pc2.Parse())
		pc2.WriteUnk();

	if (bIsUpgrade)
	{
		if (m_nArg < sizeof(Upgradable::ScheduleUpgrade))
			return; // don't care of partial result

		const auto& arg = *(const Upgradable::ScheduleUpgrade*) m_pArg;

		Env::DocAddText("", "Schedule upgrade, ");
		WriteUpgradeParams(arg);
	}

	if (m_State)
	{
		Env::DocAddText("", "Upgrade Owner=");
		Env::DocAddBlob_T("", us.m_Pk);
		Env::DocAddText("", ", ");
		WriteUpgradeParams(us);
	}
}

void ParserContext::WriteUpgradeParams(const Upgradable::Next& us)
{
	Env::DocAddText("", "cidNext=");
	Env::DocAddBlob_T("", us.m_cidNext);
	Env::DocAddText("", ", hNext=");
	Env::DocAddNum("", us.m_hNextActivate);
}

void ParserContext::On_Vault()
{
	if (m_Method && !WriteStdMethod())
	{
		switch (m_iMethod)
		{
		case Vault::Deposit::s_iMethod:
			Env::DocAddText("", "Deposit");
			if (m_nArg >= sizeof(Vault::Deposit))
			{
				const auto& arg = *(const Vault::Deposit*) m_pArg;
				Env::DocAddText("", ", User=");
				Env::DocAddBlob_T("", arg.m_Account);
			}
			break;

		case Vault::Withdraw::s_iMethod:
			// no need to include the account, it's visible in the sigs list
			Env::DocAddText("", "Withdraw");
			break;
		}
	}

	if (m_State)
	{
		// TODO: write all accounts
	}
}

void ParserContext::On_Faucet()
{
	if (m_Method)
	{
		WriteStdMethod();

		switch (m_iMethod)
		{
		case 0:
			if (m_nArg >= sizeof(Faucet::Params))
			{
				auto& pars = *(Faucet::Params*) m_pArg;
				Env::DocAddNum(", Backlog period: ", pars.m_BacklogPeriod);
				Env::DocAddNum(", Max withdraw: ", pars.m_MaxWithdraw);
			}
			break;

		case Faucet::Deposit::s_iMethod: Env::DocAddText("", "deposit"); break;
		case Faucet::Withdraw::s_iMethod: Env::DocAddText("", "withdraw"); break;
		}
	}

	if (m_State)
	{
		// TODO: write all accounts
	}
}

BEAM_EXPORT void Method_0(const ShaderID& sid, const ContractID& cid, uint32_t iMethod, const void* pArg, uint32_t nArg)
{
	ParserContext pc(sid, cid);
	pc.m_Method = true;
	pc.m_iMethod = iMethod;
	pc.m_pArg = pArg;
	pc.m_nArg = nArg;

	pc.Parse();
}

BEAM_EXPORT void Method_1(const ShaderID& sid, const ContractID& cid)
{
	ParserContext pc(sid, cid);
	pc.Parse();
}

BEAM_EXPORT void Method_2(const ShaderID& sid, const ContractID& cid)
{
	ParserContext pc(sid, cid);
	pc.m_Name = false;
	pc.m_State = true;
	pc.Parse();
}
