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

#ifndef NDN_PRODUCER_APP_H
#define NDN_PRODUCER_APP_H

#include "ns3/core-module.h"
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include <ns3/ndnSIM/apps/ndn-app.hpp>
#include <ns3/ndnSIM/ndn-cxx/util/random.hpp>

#include <map>
#include <memory>
#include <vector>

#include "ndn-timeout-app.hpp"

#include "object-container.hpp"

using namespace std;
using namespace ns3;
using namespace ns3::ndn;

class ProducerDrn : public TimeoutApp
{
public:
	static ns3::TypeId
	GetTypeId();

	ProducerDrn();

	virtual void
	StartApplication();

	virtual void
	StopApplication();

	void
	sendInterestPA(string topic);

	void
	sendInterestPU(string topic);

	virtual void
	OnInterest(shared_ptr<const Interest> interest);

	virtual void
	OnData(shared_ptr<const Data> data);

	virtual void
	OnTimeout(shared_ptr<const Interest> interest);

	virtual void
	OnNack(shared_ptr<const ::ndn::lp::Nack> nack);

	void
	initTopics();
	void
	generateTopics(Name topicPrefix, std::shared_ptr<std::vector<EventId>> eventPtr);
	void
	SendDataPublishInterest(Name &topicName);

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
	string m_nodePrefix;

	uint32_t m_nTotalDS;
	uint32_t m_nDataSize;
	double m_frequency;
	ns3::Ptr<ns3::RandomVariableStream> m_random;
	std::string m_randomType;

	uint64_t m_nGenTopic;

	::ns3::Time m_interestLifeTime; ///< \brief LifeTime for interest packet

	ns3::Ptr<ObjectContainer> m_objectContainer;

	vector<string> m_prefixList;
	std::map<std::string, std::map<uint32_t, uint32_t>> m_publishTopics;
	std::map<std::string, uint32_t> m_topicSequences;
	std::vector<std::string> m_topicLists;

	::ndn::random::RandomNumberEngine& m_rng;
	uniform_int_distribution<int> m_rangeUniformRandom;

	std::map<EventId, int> m_publishEvent;

	uint32_t m_nPAInterestCount;
	uint32_t m_nPUInterestCount;
	uint32_t m_nDPInterestCount;
	uint32_t m_nPANackCount;
	uint32_t m_nPUNackCount;
	uint32_t m_nDPNackCount;
	uint32_t m_nPADataCount;
	uint32_t m_nPUDataCount;
	uint32_t m_nDPDataCount;
};
#endif
