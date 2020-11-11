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

#include "ndn-consumer-drn.hpp"
#include <ns3/ndnSIM/model/ndn-common.hpp>
#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/random.hpp>

#include <iostream>
#include <vector>
#include <memory>

#include "utils.hpp"

using namespace ns3::ndn;

NS_LOG_COMPONENT_DEFINE("drn.ConsumerDrn");

NS_OBJECT_ENSURE_REGISTERED(ConsumerDrn);

ns3::TypeId
ConsumerDrn::GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("ConsumerDrn")
		.SetGroupName("Ndn")
		.SetParent<Application>()
		.AddConstructor<ConsumerDrn>()
		.AddAttribute("RnPrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
						MakeNameAccessor(&ConsumerDrn::m_rnPrefix), MakeNameChecker())
		.AddAttribute("TopicPrefix", "Topic list, for which producer has the data", ns3::StringValue("/"),
						MakeStringAccessor(&ConsumerDrn::m_topicPrefix), ns3::MakeStringChecker())

		.AddAttribute("NumSubscribeMessage", "Number of subscribe messages", ns3::UintegerValue(100),
						ns3::MakeUintegerAccessor(&ConsumerDrn::m_nSub), ns3::MakeUintegerChecker<uint32_t>())

		.AddAttribute("TotalDataStream", "Number of Data Stream", ns3::UintegerValue(200),
						ns3::MakeUintegerAccessor(&ConsumerDrn::m_nTotalDS), ns3::MakeUintegerChecker<uint32_t>())

		.AddAttribute("Frequency", "Frequency of interest packets", StringValue("1.0"),
						MakeDoubleAccessor(&ConsumerDrn::m_frequency), MakeDoubleChecker<double>())

		.AddAttribute("Randomize", "Type of send time randomization: none (default), uniform, exponential", StringValue("none"),
						MakeStringAccessor(&ConsumerDrn::SetRandomize, &ConsumerDrn::GetRandomize), MakeStringChecker())

		.AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("4s"),
						MakeTimeAccessor(&ConsumerDrn::m_interestLifeTime), MakeTimeChecker())

		.AddAttribute("CustomAttributes", "Custom Attributes", PointerValue (),
						ns3::MakePointerAccessor(&ConsumerDrn::m_objectContainer), 
                        ns3::MakePointerChecker<ObjectContainer>())
		;

    return tid;
}

/**
 * @brief ConsumerDrn
 */
ConsumerDrn::ConsumerDrn()
	: m_nSub(100)
	, m_nTotalDS(200)
	, m_frequency(1.0)
	, m_rng(::ndn::random::getRandomNumberEngine())
	, m_rangeUniformRandom(0, 60000)
	, m_nTSInterestCount(0)
	, m_nTMInterestCount(0)
	, m_nDMInterestCount(0)
	, m_nDRInterestCount(0)
	, m_nTSNackCount(0)
	, m_nTMNackCount(0)
	, m_nDMNackCount(0)
	, m_nDRNackCount(0)
	, m_nTSDataCount(0)
	, m_nTMDataCount(0)
	, m_nDMDataCount(0)
	, m_nDRDataCount(0)
{
}

// need to tune variables
void
ConsumerDrn::SetRandomize(const std::string& value)
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
ConsumerDrn::GetRandomize() const
{
    return m_randomType;
}

void
ConsumerDrn::StartApplication() {
	TimeoutApp::StartApplication();
	NS_LOG_DEBUG("StartApplication");

	std::vector<std::string> availableSubscriptions;
	for (uint32_t i = 0; i < m_nTotalDS; i++) {
		std::string prefixString(stringf("topic-%u", i));
		availableSubscriptions.push_back(prefixString);
	}

	m_objectContainer->get("SubscribeTopic", m_prefixList);

	vector<string>::iterator iter = m_prefixList.begin();
	for (; iter != m_prefixList.end(); iter ++) {
		int at = (*iter).find('*');
		if (at < 0) {
			std::shuffle(availableSubscriptions.begin(), availableSubscriptions.end(), m_rng);

			std::vector<std::string> topicLists;
			for (uint32_t i = 0; i < m_nSub; i++) {
				std::string topic(*iter + "/" + availableSubscriptions[i]);
				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestTS, this, topic);
			}
		} else {
			ns3::Time delay(ns3::MilliSeconds(0));
			ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestTS, this, *iter);
		}
	}
}

void
ConsumerDrn::StopApplication() {
	NS_LOG_DEBUG("StopApplication");

	TimeoutApp::StopApplication();

	NS_LOG_INFO(stringf("Send TSInterestCount: %5u", m_nTSInterestCount));
	NS_LOG_INFO(stringf("Send TMInterestCount: %5u", m_nTMInterestCount));
	NS_LOG_INFO(stringf("Send DMInterestCount: %5u", m_nDMInterestCount));
	NS_LOG_INFO(stringf("Send DRInterestCount: %5u", m_nDRInterestCount));
	NS_LOG_INFO(stringf("Nack TSInterestCount: %5u", m_nTSNackCount));
	NS_LOG_INFO(stringf("Nack TMInterestCount: %5u", m_nTMNackCount));
	NS_LOG_INFO(stringf("Nack DMInterestCount: %5u", m_nDMNackCount));
	NS_LOG_INFO(stringf("Nack DRInterestCount: %5u", m_nDRNackCount));
	NS_LOG_INFO(stringf("Recv TSDataCount:     %5u", m_nTSDataCount));
	NS_LOG_INFO(stringf("Recv TMDataCount:     %5u", m_nTMDataCount));
	NS_LOG_INFO(stringf("Recv DMDataCount:     %5u", m_nDMDataCount));
	NS_LOG_INFO(stringf("Recv DRDataCount:     %5u", m_nDRDataCount));
}

void
ConsumerDrn::sendInterestTS(string topicPrefix) {
	// RN/PA/a/b/c
	::ndn::Name interestName(m_rnPrefix);
	interestName.append("TS").append(topicPrefix);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	NDN_LOG_DEBUG("send interest: " << interestName);

#if 0
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nTSInterestCount += 1;
}

void
ConsumerDrn::sendInterestTM(string topicPrefix) {
	// RN/TM/a/b/c
	::ndn::Name interestName(m_rnPrefix);
	interestName.append("TM").append(topicPrefix);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	NDN_LOG_DEBUG("send interest: " << interestName);

#if 0
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nTMInterestCount += 1;
}

void
ConsumerDrn::sendInterestDM(string nodeName, string topicPrefix) {
	// RN/PA/a/b/c
	::ndn::Name interestName(nodeName);
	interestName.append("DM").append(topicPrefix);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	NDN_LOG_DEBUG("send interest: " << interestName);

#if 0
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nDMInterestCount += 1;
}

void
ConsumerDrn::sendInterestDR(string nodeName, string topicPrefix) {
	// RN/PA/a/b/c
	::ndn::Name interestName(nodeName);
	interestName.append("DR").append(topicPrefix);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	interest->setInterestLifetime(interestLifeTime);

	NDN_LOG_DEBUG("send interest: " << interestName);

#if 0
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nDRInterestCount += 1;
}

void
ConsumerDrn::OnTimeout(shared_ptr<const Interest> interest) {
	TimeoutApp::OnTimeout(interest);

	const Name &interestName = interest->getName();
	int32_t prefixSize = m_rnPrefix.size();
	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("TS") == 0) {
		m_nTSInterestCount += 1;
	} else	if (cmd.compare("TM") == 0) {
		m_nTMInterestCount += 1;
	} else	if (cmd.compare("DM") == 0) {
		m_nDMInterestCount += 1;
	} else	if (cmd.compare("DR") == 0) {
		m_nDRInterestCount += 1;
	}
	NS_LOG_DEBUG(interestName);
}

void
ConsumerDrn::OnInterest(shared_ptr<const Interest> interest) {
	TimeoutApp::OnInterest(interest);
}

void
ConsumerDrn::OnData(shared_ptr<const Data> data) {
	TimeoutApp::OnData(data);

	const Name &dataName = data->getName();
	NS_LOG_DEBUG("recv data: " << dataName);

	uint32_t rnPrefixSize = m_rnPrefix.size();
	Name prefix = dataName.getSubName(0, rnPrefixSize);
	if (prefix.equals(m_rnPrefix)) {
		string cmd = dataName.get(rnPrefixSize).toUri();
		if (cmd.compare("TS") == 0) {
			m_nTSDataCount += 1;

			int32_t attentionIndex = rnPrefixSize + 1;
			Name topicName = dataName.getSubName(attentionIndex, Name::npos);

			// data /RN/TS/a/b/* 를 받음
			ns3::Time delay(ns3::MilliSeconds(0));
			ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestTM, this, topicName.toUri());

            /*
			string topic = topicName.toUri();
			std::stringstream sio;
			StringMapIterator mapIter;
			int at = topic.find('*');
			if (at < 0) {
				std::shuffle(m_topicLists.begin(), m_topicLists.end(), m_rng);
				for (uint32_t i = 0; i < m_topicLists.size() && i < m_nSub; i += 1) {
					Name qualifiedName(topicName);
					qualifiedName.append(m_topicLists[i]);

					NS_LOG_DEBUG("send interest: " << qualifiedName);

					ns3::Time delay(ns3::MilliSeconds(0));
					ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestTM, this, qualifiedName.toUri());
				}
			} else {
				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestTM, this, topicName.toUri());
			}
            */
		} else if (cmd.compare("TM") == 0) {
			m_nTMDataCount += 1;

			// /RN-yyyyy/TM/a/b/c : (/a/b/c, RN-00001)
			// /RN-yyyyy/TM/a/b/* : (/a/b/c, RN-00001)|(/a/b/d, RN-00002)
			int32_t attentionIndex = rnPrefixSize + 1;
			Name topicName = dataName.getSubName(attentionIndex, Name::npos);
			string topic = topicName.toUri();

			const Block &content = data->getContent();
			if (0 < content.elements_size()) {
				content.parse();
			}

			shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)content.value(), content.value_size());

			string producerStr((const char *)content.value(), content.value_size());
			vector<string> producerList = split(producerStr, ',');

			StringListIterator stringIterator;

			stringIterator = producerList.begin();
			for (;stringIterator != producerList.end(); stringIterator ++) {
				std::pair<std::string, std::string> pair = parsePair(*stringIterator, ':');

				NDN_LOG_DEBUG(pair.first << " : " << pair.second);
			}

			stringIterator = producerList.begin();
			for (;stringIterator != producerList.end(); stringIterator ++) {
				if (stringIterator->size() == 0) {
					continue;
				}
				std::pair<std::string, std::string> pair = parsePair(*stringIterator, ':');

				StringListMapIterator stringListPtrIter;
				StringListPtr stringListPtr;
				stringListPtrIter = m_publishNodes.find(pair.first);
				if (stringListPtrIter == m_publishNodes.end()) {
					stringListPtr = make_shared<std::vector<string>>();
					m_publishNodes.insert(std::pair<string, StringListPtr>(pair.first, stringListPtr));
				} else {
					stringListPtr = stringListPtrIter->second;
				}
				stringListPtr->push_back(pair.second);

				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestDM, this, pair.second, pair.first);
			}
		}
	} else {
		// RN-yyyyy/{cmd}/...
		uint32_t rnPrefixSize = 1;
		string cmd = dataName.get(rnPrefixSize).toUri();
		if (cmd.compare("DM") == 0) {
			m_nDMDataCount += 1;

			int32_t attentionIndex = rnPrefixSize + 1;
			Name nodeName(dataName.getPrefix(rnPrefixSize));
			Name topicName = dataName.getSubName(attentionIndex, Name::npos);
			string topic = topicName.toUri();

			const Block &content = data->getContent();
			if (0 < content.elements_size()) {
				content.parse();
			}

#if 0
			Block::element_const_iterator iter = content.elements_begin();
			for (; iter != content.elements_end(); iter ++) {
				string qualified((const char *)iter->value(), iter->value_size());
				NDN_LOG_DEBUG(qualified);

				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestDR, this, nodeName.toUri(), qualified);
			}
#else
			string sequence((const char *)content.value(), content.value_size());

			Name qualifiedName(topicName);
			qualifiedName.append(sequence);

			NDN_LOG_DEBUG("Data Manifest: " << qualifiedName);
#if 0
			ns3::Time delay(ns3::MilliSeconds(0));
			ns3::Simulator::Schedule(delay, &ConsumerDrn::sendInterestDR, this, nodeName.toUri(), qualifiedName.toUri());
#else
			sendInterestDR(nodeName.toUri(), qualifiedName.toUri());
#endif
#endif
			Time nextTMDelay;
			if (m_random == 0) {
				nextTMDelay = Seconds(m_frequency);
			} else {
				nextTMDelay = Seconds(m_random->GetValue());
			}
#if 0
			ns3::Simulator::Schedule(nextTMDelay, &ConsumerDrn::sendInterestTM, this, topicName.toUri());
#else
			sendInterestTM(topicName.toUri());
#endif
		} else if (cmd.compare("DR") == 0) {
			m_nDRDataCount += 1;

			int32_t attentionIndex = rnPrefixSize + 1;
			Name topicName = dataName.getSubName(attentionIndex, Name::npos);
			string topic = topicName.toUri();

			const Block &content = data->getContent();
			if (0 < content.elements_size()) {
				content.parse();
			}

            NDN_LOG_INFO("RevcDRTopic: " << topic);
			NDN_LOG_DEBUG(stringf("Content size: %d", content.value_size()));
		}
	}
}

void
ConsumerDrn::OnNack(shared_ptr<const ::ndn::lp::Nack> nack) {
	TimeoutApp::OnNack(nack);

	NDN_LOG_DEBUG(nack->getInterest().getName());

	const Name &interestName = nack->getInterest().getName();
	int32_t prefixSize = m_rnPrefix.size();
	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("TS") == 0) {
		m_nTSNackCount += 1;
	} else	if (cmd.compare("TM") == 0) {
		m_nTMNackCount += 1;
	} else	if (cmd.compare("DM") == 0) {
		m_nDMNackCount += 1;
	} else	if (cmd.compare("DR") == 0) {
		m_nDRNackCount += 1;
	}
}
