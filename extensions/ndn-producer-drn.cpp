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

#include "ndn-producer-drn.hpp"

#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/random.hpp>

//#include <ndn-cxx/util/random.hpp>
#include <vector>
#include <iostream>
#include <algorithm>

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include "utils.hpp"

using namespace std;
using namespace ns3::ndn;

NS_LOG_COMPONENT_DEFINE("drn.ProducerDrn");

NS_OBJECT_ENSURE_REGISTERED(ProducerDrn);

ns3::TypeId
ProducerDrn::GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("ProducerDrn")
		.SetGroupName("Ndn")
		.SetParent<Application>()
		.AddConstructor<ProducerDrn>()
		.AddAttribute("RnPrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
						MakeNameAccessor(&ProducerDrn::m_rnPrefix), MakeNameChecker())
		.AddAttribute("TopicPrefix", "Topic list, for which producer has the data", ns3::StringValue("/"),
						MakeStringAccessor(&ProducerDrn::m_topicPrefix), ns3::MakeStringChecker())
		.AddAttribute("NodePrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
						MakeStringAccessor(&ProducerDrn::m_nodePrefix), ns3::MakeStringChecker())

		.AddAttribute("TotalDataStream", "Number of Data Stream", ns3::UintegerValue(200),
						ns3::MakeUintegerAccessor(&ProducerDrn::m_nTotalDS), ns3::MakeUintegerChecker<uint32_t>())

		.AddAttribute("DataSize", "Size of messages", ns3::UintegerValue(100),
						ns3::MakeUintegerAccessor(&ProducerDrn::m_nDataSize), ns3::MakeUintegerChecker<uint32_t>())

		.AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
						MakeDoubleAccessor(&ProducerDrn::m_frequency), MakeDoubleChecker<double>())

		.AddAttribute("Randomize", "Type of send time randomization: none (default), uniform, exponential", StringValue("none"),
						MakeStringAccessor(&ProducerDrn::SetRandomize, &ProducerDrn::GetRandomize), MakeStringChecker())

		.AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("4s"),
				        MakeTimeAccessor(&ProducerDrn::m_interestLifeTime), MakeTimeChecker())

		.AddAttribute("CustomAttributes", "Custom Attributes", PointerValue (),
						ns3::MakePointerAccessor(&ProducerDrn::m_objectContainer), ns3::MakePointerChecker<ObjectContainer>())
						;

    return tid;
}

/**
 * @brief ProducerDrn
 */
ProducerDrn::ProducerDrn()
	: m_nTotalDS(200)
	, m_nDataSize(100)
	, m_frequency(1.0)
	, m_nGenTopic(0)
	, m_rng(::ndn::random::getRandomNumberEngine())
	, m_rangeUniformRandom(0, 60000)
	, m_nPAInterestCount(0)
	, m_nPUInterestCount(0)
	, m_nDPInterestCount(0)
	, m_nPANackCount(0)
	, m_nPUNackCount(0)
	, m_nDPNackCount(0)
	, m_nPADataCount(0)
	, m_nPUDataCount(0)
	, m_nDPDataCount(0)
{
}

// need to tune variables
void
ProducerDrn::SetRandomize(const std::string& value)
{
    if (value == "uniform") {
        m_random = CreateObject<UniformRandomVariable>();
        m_random->SetAttribute("Min", DoubleValue(1.0 * m_frequency));
        m_random->SetAttribute("Max", DoubleValue(5.0 * m_frequency));
    }
    else if (value == "exponential") {
        m_random = CreateObject<ExponentialRandomVariable>();
        m_random->SetAttribute("Mean", DoubleValue(2.5 * m_frequency));
        m_random->SetAttribute("Bound", DoubleValue(5.0 * m_frequency));
    }
    else
        m_random = 0;

    m_randomType = value;
}

std::string
ProducerDrn::GetRandomize() const
{
    return m_randomType;
}

void
ProducerDrn::StartApplication() {
	TimeoutApp::StartApplication();
	NS_LOG_DEBUG("StartApplication");

	FibHelper::AddRoute(GetNode(), m_nodePrefix, m_face, 0);
	NS_LOG_DEBUG(stringf("AddFIB(%5u): ", GetNode()->GetId()) << m_nodePrefix);

	m_objectContainer->get("PublishTopic", m_prefixList);

	initTopics();
}

void
ProducerDrn::StopApplication() {
	std::map<EventId, int>::iterator iter;
	for (iter = m_publishEvent.begin(); iter != m_publishEvent.end(); iter ++) {
		Simulator::Cancel(iter->first);
	}
	m_publishEvent.clear();

	for (uint32_t i = 0; i < m_topicLists.size(); i += 1) {
		ns3::Time delay(ns3::MilliSeconds(0));
		Simulator::Schedule(Seconds(0.0), &ProducerDrn::sendInterestPU, this, m_topicLists[i]);
	}

	NDN_LOG_INFO("StopApplication");
	// print counters
	TimeoutApp::StopApplication();

	NS_LOG_INFO(stringf("Send PAInterestCount: %5u", m_nPAInterestCount));
	NS_LOG_INFO(stringf("Send PUInterestCount: %5u", m_nPUInterestCount));
	NS_LOG_INFO(stringf("Send DPInterestCount: %5u", m_nDPInterestCount));
	NS_LOG_INFO(stringf("Recv PADataCount:     %5u", m_nPADataCount));
	NS_LOG_INFO(stringf("Recv PUDataCount:     %5u", m_nPUDataCount));
	NS_LOG_INFO(stringf("Recv DPDataCount:     %5u", m_nDPDataCount));
}

void
ProducerDrn::initTopics() {

	std::vector<std::string> availableSubscriptions;
	for (uint32_t i = 0; i < m_nTotalDS; i++) {
		std::string prefixString(stringf("topic-%u", i));
		availableSubscriptions.push_back(prefixString);
	}

	std::shuffle(availableSubscriptions.begin(), availableSubscriptions.end(), m_rng);

	// topic advertise
	vector<string>::iterator iter = m_prefixList.begin();
	for (; iter != m_prefixList.end(); iter ++) {
		for (uint32_t i = 0; i < availableSubscriptions.size(); i += 1) {
			Name topic(*iter);
			topic.append(availableSubscriptions[i]);

			m_topicLists.push_back(topic.toUri());

			m_topicSequences[topic.toUri()] = 0;

			NDN_LOG_DEBUG("init topic sequence: " << topic.toUri());
		}
	}

	// topic advertise
	for (uint32_t i = 0; i < m_topicLists.size(); i += 1) {
		Name topic(m_topicLists[i]);
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &ProducerDrn::sendInterestPA, this, topic.toUri());
	}
}

void
ProducerDrn::sendInterestPA(string topic) {
	// RN/PA/a/b/c
	::ndn::Name interestName(m_rnPrefix);
	interestName.append("PA").append(topic);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	NDN_LOG_DEBUG("send interest: " << interestName);

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nPAInterestCount += 1;
}

void
ProducerDrn::sendInterestPU(string topic) {
	// RN/PA/a/b/c
	::ndn::Name interestName(m_rnPrefix);
	interestName.append("PU").append(topic);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	NDN_LOG_DEBUG("send interest: " << interestName);

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nPUInterestCount += 1;
}

void
ProducerDrn::generateTopics(Name topicPrefix, std::shared_ptr<std::vector<EventId>> eventPtr)
{
	m_publishEvent.erase((*eventPtr)[0]);

	std::string tempName(topicPrefix.toUri());
	uint32_t tempSeq = m_topicSequences[tempName];
	m_topicSequences[tempName] = tempSeq + 1;

	std::map<std::string, std::map<uint32_t, uint32_t>>::iterator publishIter;
	publishIter = m_publishTopics.find(tempName);
	if (publishIter == m_publishTopics.end()) {
		m_publishTopics.insert(std::make_pair(tempName, std::map<uint32_t, uint32_t>()));
		m_publishTopics[tempName].insert(std::make_pair(tempSeq, 0)); // seq 100번째 topic은 0번 publish되었다.
	} else {
		publishIter->second.insert(std::make_pair(tempSeq, 0));
	}

	Name newTopicNameSeq = Name(tempName).append(stringf("%u", tempSeq));
	NDN_LOG_DEBUG("newTopicNameSeq : " << newTopicNameSeq);

	m_nGenTopic++;
	// topic published time
	NDN_LOG_INFO("PublishTopic: " << newTopicNameSeq);

	SendDataPublishInterest(newTopicNameSeq);
    //Simulator::Schedule(Seconds(0.0), &ProducerDrn::SendDataPublishInterest, this, newTopicNameSeq);
}

void
ProducerDrn::SendDataPublishInterest(Name &topicName)
{
	if (!m_active) {
		return;
	}

	// /RN/DP/a/b/c/topic-80/0
	Name interestName(m_rnPrefix);
	interestName.append("DP");
	interestName.append(topicName);
	NS_LOG_INFO("SentDataPublishInterest: " << interestName);

	// Create and configure ndn::Interest
	shared_ptr<Interest> interest = std::make_shared<Interest>();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	shared_ptr<::ndn::Buffer> buffer = make_shared<::ndn::Buffer>(m_nDataSize);
	::memset(buffer->get<uint8_t>(), 'a', buffer->size());

	::ndn::Block params(::ndn::tlv::AppPrivateBlock1, buffer);
	params.encode();
	interest->setParameters(params);

	NS_LOG_DEBUG("Sending Interest packet for " << *interest);

	// Call trace (for logging purposes)
	m_transmittedInterests(interest, this, m_face);

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nDPInterestCount += 1;
}

void
ProducerDrn::OnTimeout(shared_ptr<const Interest> interest) {
	TimeoutApp::OnTimeout(interest);

	const Name &interestName = interest->getName();
	int32_t prefixSize = m_rnPrefix.size();
	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("PA") == 0) {
		m_nPAInterestCount += 1;
	}
	NS_LOG_DEBUG(interestName);
}

void
ProducerDrn::OnInterest(shared_ptr<const Interest> interest) {
	TimeoutApp::OnInterest(interest);
}

void
ProducerDrn::OnData(shared_ptr<const Data> data) {
	TimeoutApp::OnData(data);

	const Name &dataName = data->getName();
	NS_LOG_DEBUG("recv data: " << dataName);

	int32_t prefixSize = m_rnPrefix.size();
	Name prefix = dataName.getSubName(0, prefixSize);

	// /RN/ 요청 interest에 대한 응답 Data
	if (prefix.equals(m_rnPrefix)) {
		string cmd = dataName.get(prefixSize).toUri();
		// /RN/PA/a/b/c/topic-{nnn}
		if (cmd.compare("PA") == 0) {
			m_nPADataCount += 1;

			int32_t attentionIndex = prefixSize + 1;
			Name topicPrefix = dataName.getSubName(attentionIndex, Name::npos);

			m_topicSequences[topicPrefix.toUri()] = 0;

			Time t;
			if (m_random == 0) {
				t = Seconds(m_frequency);
			} else {
				t = Seconds(m_random->GetValue());
			}

			std::shared_ptr<std::vector<EventId>> eventPtr = std::make_shared<std::vector<EventId>>();
			EventId eventId = Simulator::Schedule(t, &ProducerDrn::generateTopics, this, topicPrefix, eventPtr);
			eventPtr->push_back(eventId);
			m_publishEvent.insert(std::pair<EventId, int>(eventId, 0));
		} else if (cmd.compare("PU") == 0) {
			m_nPUDataCount += 1;
		} else if (cmd.compare("DP") == 0) {
			m_nDPDataCount += 1;

			int32_t attentionIndex = prefixSize + 1;
			Name prefix = dataName.getSubName(attentionIndex, Name::npos);

			Name qualifiedName = dataName.getSubName(2, dataName.size()-1);
			Name topicName = qualifiedName.getSubName(0, qualifiedName.size()-1);
			string topicPrefix = topicName.toUri();

			// schedule next topic generation
			Time t;
			if (m_random == 0) {
				t = Seconds(m_frequency);
			} else {
				t = Seconds(m_random->GetValue());
			}

			std::shared_ptr<std::vector<EventId>> eventPtr = std::make_shared<std::vector<EventId>>();
			EventId eventId = Simulator::Schedule(t, &ProducerDrn::generateTopics, this, topicPrefix, eventPtr);
			eventPtr->push_back(eventId);
			m_publishEvent.insert(std::pair<EventId, int>(eventId, 0));
		}
	}
}

void
ProducerDrn::OnNack(shared_ptr<const ::ndn::lp::Nack> nack) {
	NDN_LOG_DEBUG(nack->getInterest().getName());
	TimeoutApp::OnNack(nack);

	NDN_LOG_DEBUG(nack->getInterest().getName());

	const Name &interestName = nack->getInterest().getName();
	int32_t prefixSize = m_rnPrefix.size();
	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("PA") == 0) {
		m_nPANackCount += 1;
	} else	if (cmd.compare("PU") == 0) {
		m_nPUNackCount += 1;
	} else	if (cmd.compare("DP") == 0) {
		m_nDPNackCount += 1;
	}
}
