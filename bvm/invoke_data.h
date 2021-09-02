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
#pragma once

#include <core/block_crypt.h>
#include <map>

namespace beam::bvm2 {

	struct FundsMap: public std::map<Asset::ID, AmountSigned>
	{
		void AddSpend(Asset::ID aid, AmountSigned val);
		void operator += (const FundsMap&);
	};

	struct ContractInvokeEntry
	{
		ECC::uintBig m_Cid;
		uint32_t m_iMethod;
		ByteBuffer m_Data;
		ByteBuffer m_Args;
		std::vector<ECC::Hash::Value> m_vSig;
		uint32_t m_Charge;

		static const uint32_t s_ChargeAdv = static_cast<uint32_t>(-1);

		bool IsAdvanced() const {
			return s_ChargeAdv == m_Charge;
		}

		struct Advanced
		{
			HeightRange m_Height;
			Amount m_Fee;
			ECC::Point m_ptFullBlind;
			ECC::Point m_ptFullNonce;
			ECC::Hash::Value m_hvBlind;
			ECC::Hash::Value m_hvNonce;
			ECC::Scalar m_kForeignSig;

			std::vector<ECC::Point> m_vPks;

		} m_Adv;

		FundsMap m_Spend; // ins - outs, not including fee
		std::string m_sComment;

		template <typename Archive>
		void serialize(Archive& ar)
		{
			ar
				& m_iMethod
				& m_Args
				& m_vSig
				& m_Charge
				& m_sComment
				& Cast::Down< std::map<Asset::ID, AmountSigned> >(m_Spend);

			if (m_iMethod)
				ar & m_Cid;
			else
			{
				m_Cid = Zero;
				ar & m_Data;
			}

			if (IsAdvanced())
			{
				ar
					& m_Adv.m_Height
					& m_Adv.m_Fee
					& m_Adv.m_ptFullBlind
					& m_Adv.m_ptFullNonce
					& m_Adv.m_hvBlind
					& m_Adv.m_hvNonce
					& m_Adv.m_kForeignSig
					& m_Adv.m_vPks;
			}
		}

		void Generate(Transaction&, Key::IKdf&, const HeightRange& hr, Amount fee) const;

		void Generate(std::unique_ptr<TxKernelContractControl>&, ECC::Scalar::Native& sk, Key::IKdf&, const HeightRange& hr, Amount fee, ECC::Scalar* pE, bool bSign) const;

		[[nodiscard]] Amount get_FeeMin(Height) const;

	private:

		void get_SigPreimage(ECC::Hash::Value&, const ECC::Hash::Value& krnMsg) const;
	};

	typedef std::vector<ContractInvokeEntry> ContractInvokeData;

	std::string getFullComment(const ContractInvokeData&);
	beam::Amount getFullFee(const ContractInvokeData&, Height);
	bvm2::FundsMap getFullSpend(const ContractInvokeData&);
}
