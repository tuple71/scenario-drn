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

#ifndef NDN_CONSUMER_APP_H
#define NDN_CONSUMER_APP_H

#include <string>
#include <memory>
#include "ns3/core-module.h"

#include <ns3/ndnSIM/apps/ndn-app.hpp>
//#include <ns3/ndnSIM/ndn-cxx/name.hpp>
//#include <ns3/ndnSIM/ndn-cxx/face.hpp>
#include <ns3/ndnSIM/ndn-cxx/util/random.hpp>

#include "object-container.hpp"

#include "ndn-timeout-app.hpp"

#include "utils.hpp"

using namespace ns3;
using namespace ns3::ndn;
using namespace std;

class ConsumerDrn : public TimeoutApp
{
public:
	static ns3::TypeId
	GetTypeId();

	ConsumerDrn();

	virtual void
	StartApplication();

	virtual void
	StopApplication();

	void
	sendInterestTS(string topicPrefix);

	void
	sendInterestTM(string topicPrefix);

	void
	sendInterestDM(string nodeName, string topicPrefix);

	void
	sendInterestDR(string nodeName, string topicPrefix);

	virtual void
	OnTimeout(shared_ptr<const Interest> interest);

	virtual void
	OnInterest(shared_ptr<const Interest> interest);

	virtual void
	OnData(shared_ptr<const Data> data);

	virtual void
	OnNack(shared_ptr<const ::ndn::lp::Nack> nack);

	/**
	 * @brief Set type of frequency randomization
	 * @param value Either 'none', 'uniform', or 'exponential'
	 */
	void
	SetRandomize(const std::string& value);

	/**
	 * @brief Get type of frequency randomization
	 * @returns either 'none', 'uniform', or 'exponential'
	 */
	std::string
	GetRandomize() const;

private:
	::ndn::Name m_rnPrefix;
	string m_topicPrefix;
	uint32_t m_nSub;
	uint32_t m_nTotalDS;
	::ns3::Time m_interestLifeTime; ///< \brief LifeTime for interest packet
	double m_frequency;
	ns3::Ptr<ns3::RandomVariableStream> m_random;
	std::string m_randomType;

	std::map<std::string, StringListPtr> m_topicMap;
	std::map<std::string, StringListPtr> m_publishNodes;
	vector<string> m_prefixList;
	ns3::Ptr<ObjectContainer> m_objectContainer;

	::ndn::random::RandomNumberEngine& m_rng;
	uniform_int_distribution<int> m_rangeUniformRandom;

	uint32_t m_nTSInterestCount;
	uint32_t m_nTMInterestCount;
	uint32_t m_nDMInterestCount;
	uint32_t m_nDRInterestCount;
	uint32_t m_nTSNackCount;
	uint32_t m_nTMNackCount;
	uint32_t m_nDMNackCount;
	uint32_t m_nDRNackCount;
	uint32_t m_nTSDataCount;
	uint32_t m_nTMDataCount;
	uint32_t m_nDMDataCount;
	uint32_t m_nDRDataCount;
};

#endif
