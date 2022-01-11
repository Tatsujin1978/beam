#pragma once

namespace DaoVote
{
    static const ShaderID s_SID = { 0xb5,0x43,0x84,0x6d,0x6e,0xa0,0x8f,0x1d,0x70,0x29,0x13,0xc7,0x96,0x11,0x00,0x64,0xd5,0x58,0x54,0x7a,0x63,0xfe,0xbe,0x80,0x1a,0x35,0x38,0x55,0x01,0x6a,0x56,0xba };

#pragma pack (push, 1)

    struct Tags
    {
        static const uint8_t s_State = 0;
        // don't use tag=1 for multiple data entries, it's used by Upgradable2
        static const uint8_t s_Proposal = 2;
        static const uint8_t s_User = 3;
        static const uint8_t s_Dividend = 4;
    };

    struct Cfg
    {
        uint32_t m_hEpochDuration;
        AssetID m_Aid;
        PubKey m_pkAdmin;
    };

    struct Proposal
    {
        typedef uint32_t ID;

        struct Key {
            uint8_t m_Tag = Tags::s_Proposal;
            ID m_ID;
        };

        static const uint32_t s_VariantsMax = 64;
        static const uint32_t s_ProposalsPerEpochMax = 50;

        // followed by variants
    };

    struct ProposalMax
        :public Proposal
    {
        Amount m_pVariant[s_VariantsMax];
    };

    struct AssetAmount {
        AssetID m_Aid;
        Amount m_Amount;
    };

    struct Dividend0
    {
        struct Key {
            uint8_t m_Tag = Tags::s_Dividend;
            uint32_t m_iEpoch;
        };

        Amount m_Stake;

        // followed by array of AssetAmount
        static const uint32_t s_AssetsMax = 64;
    };

    struct DividendMax
        :public Dividend0
    {
        AssetAmount m_pArr[s_AssetsMax];
    };

    struct State
    {
        Cfg m_Cfg;
        Height m_hStart;

        uint32_t get_Epoch() const {
            auto dh = Env::get_Height() - m_hStart;
            return 1u + static_cast<uint32_t>(dh / m_Cfg.m_hEpochDuration);
        }

        Proposal::ID m_iLastProposal;

        struct Current {
            uint32_t m_iEpoch;
            uint32_t m_Proposals;
            uint32_t m_iDividendEpoch; // set to 0 if no reward
            Amount m_Stake;
        } m_Current;

        struct Next {
            uint32_t m_Proposals;
            uint32_t m_iDividendEpoch;
        } m_Next;
    };

    struct User
    {
        struct Key {
            uint8_t m_Tag = Tags::s_User;
            PubKey m_pk;
        };

        uint32_t m_iEpoch;
        uint32_t m_iDividendEpoch;
        Amount m_Stake;
        Amount m_StakeNext;

        // followed by the votes for the epoch's proposal

    };

    struct Events
    {
        struct Tags
        {
            static const uint8_t s_Proposal = 0;
        };

        struct Proposal
        {
            struct Key {
                uint8_t m_Tag = Tags::s_Proposal;
                DaoVote::Proposal::ID m_ID_be; // in big endian form, for more convenient enumeration
            };

            uint32_t m_Variants;
            // followed by arbitrary text
        };
    };

    namespace Method
    {
        struct Create
        {
            static const uint32_t s_iMethod = 0; // Ctor
            Cfg m_Cfg;
        };

        struct AddProposal
        {
            static const uint32_t s_iMethod = 3;

            uint32_t m_TxtLen;
            Events::Proposal m_Data;
            // followed by text
        };

        struct MoveFunds
        {
            static const uint32_t s_iMethod = 4;
            PubKey m_pkUser;
            Amount m_Amount;
            uint8_t m_Lock; // or unlock
        };

        struct Vote
        {
            static const uint32_t s_iMethod = 5;
            PubKey m_pkUser;
            uint32_t m_iEpoch;
            // followed by appropriate vote per proposal
        };

        struct AddDividend
        {
            static const uint32_t s_iMethod = 6;
            AssetAmount m_Val;
        };

        struct GetResults
        {
            static const uint32_t s_iMethod = 7;
            Proposal::ID m_ID;
            uint32_t m_Variants; // in/out
            uint8_t m_Finished;
            // followed by variants
        };
    }
#pragma pack (pop)

} // namespace DaoVote