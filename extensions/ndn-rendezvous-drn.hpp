/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019,  The University of Memphis
 *
 * This file is part of PSync.
 * See AUTHORS.md for complete list of PSync authors and contributors.
 *
 * PSync is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PSync is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * PSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_RENDEZVOUS_APP_H
#define NDN_RENDEZVOUS_APP_H

#include <map>
#include <string>
#include <memory>
#include <ns3/ndnSIM/apps/ndn-app.hpp>
//#include <ns3/ndnSIM/ndn-cxx/name.hpp>
#include <ns3/ndnSIM/ndn-cxx/util/random.hpp>
#include <ns3/ndnSIM/ndn-cxx/encoding/buffer.hpp>

#include "Chord-DHT/nodeInformation.hpp"
#include "rendezvous-dht.hpp"

#include "utils.hpp"

using namespace std;
using namespace ns3;
using namespace ns3::ndn;

class RendezvousDrn : public App
{
public:
	static ns3::TypeId
	GetTypeId();

	RendezvousDrn();

	virtual void
	StartApplication();

	virtual void
	StopApplication();

	void
	sendInterestJoin();

	void
	sendDataTM(const Name &interestName, int32_t attentionIndex);

	void
	sendDataPA(const Name &interestName, int32_t attentionIndex);

	virtual void
	OnInterestRN(shared_ptr<const Interest> interest);
	virtual void
	OnInterestRNXXX(shared_ptr<const Interest> interest);
	virtual void
	OnInterest(shared_ptr<const Interest> interest);

	virtual void
	OnDataRN(shared_ptr<const Data> data);
	virtual void
	OnDataRNXXX(shared_ptr<const Data> data);
	virtual void
	OnData(shared_ptr<const Data> data);

	virtual void
	OnNack(shared_ptr<const ::ndn::lp::Nack> nack);

private:
	::ndn::Name m_rnPrefix;
	::ndn::Name m_drnPrefix;
	uint32_t m_nSub;
	uint32_t m_maxNode;
	::ndn::Name m_predecessor;

	std::unique_ptr<RendezvousDHT> m_rnDht;
	::ndn::random::RandomNumberEngine& m_rng;
	uint32_t m_appId;

	::ns3::Time m_interestLifeTime; ///< \brief LifeTime for interest packet

	uint32_t m_signature;
	Name m_keyLocator;

	// RN-00001/PA/a/b/c[RN-00002]
	std::map<std::string, std::string> m_PAMap;

	// RN/DP/a/b/c/topic-0
	std::map<std::string, BufferListPtr> m_DPMap;
};

#endif
