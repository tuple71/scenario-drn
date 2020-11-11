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

#include <iostream>
#include <vector>
#include <regex>
#include <sstream>

#include "ns3/string.h"
#include "ns3/uinteger.h"
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/util/random.hpp>

#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

#include <ns3/ndnSIM/model/ndn-common.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ns3/ndnSIM/ndn-cxx/encoding/buffer.hpp>

//#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include "Chord-DHT/helperClass.h"
#include "ndn-consumer-drn.hpp"
#include "ndn-rendezvous-drn.hpp"

#include "utils.hpp"


using namespace ns3::ndn;

NS_LOG_COMPONENT_DEFINE("drn.RendezvousDrn");

NS_OBJECT_ENSURE_REGISTERED(RendezvousDrn);

ns3::TypeId
RendezvousDrn::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("RendezvousDrn")
						.SetGroupName("Ndn")
						.SetParent<Application>()
						.AddConstructor<RendezvousDrn>()
						.AddAttribute("RnPrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
										MakeNameAccessor(&RendezvousDrn::m_rnPrefix), MakeNameChecker())

						.AddAttribute("DRnPrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
										MakeNameAccessor(&RendezvousDrn::m_drnPrefix), MakeNameChecker())

						.AddAttribute("NumSubscribeMessage", "Number of subscribe messages", ns3::UintegerValue(100),
										ns3::MakeUintegerAccessor(&RendezvousDrn::m_nSub), ns3::MakeUintegerChecker<uint32_t>())

						.AddAttribute("Predecessor", "Predecessor", ns3::StringValue(""),
										MakeNameAccessor(&RendezvousDrn::m_predecessor), MakeNameChecker())

						.AddAttribute("MaxNode", "Number of rendezvous node(ring size)", ns3::UintegerValue(10),
										ns3::MakeUintegerAccessor(&RendezvousDrn::m_maxNode), ns3::MakeUintegerChecker<uint32_t>())

						.AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("4s"),
										MakeTimeAccessor(&RendezvousDrn::m_interestLifeTime), MakeTimeChecker())
							;

  return tid;
}

RendezvousDrn::RendezvousDrn()
	: m_nSub(0)
	, m_rng(::ndn::random::getRandomNumberEngine())
	, m_appId(std::numeric_limits<uint32_t>::max())
	, m_signature(0U)
{
}

void
RendezvousDrn::StartApplication() {
	App::StartApplication();

	FibHelper::AddRoute(GetNode(), m_drnPrefix, m_face, 0);
	FibHelper::AddRoute(GetNode(), m_rnPrefix, m_face, 0);
	NS_LOG_DEBUG(stringf("AddFIB(%5u): ", GetNode()->GetId()) << m_drnPrefix);

	m_rnDht.reset(new RendezvousDHT(m_drnPrefix, m_predecessor, m_appLink, m_maxNode));
/*
	 // This starts the consumer side by sending a hello interest to the producer
	 // When the producer responds with hello data, afterReceiveHelloData is called
	 m_rnTopic->sendHelloInterest();
*/

	if(m_drnPrefix.equals(m_predecessor)) {
		m_rnDht->create();
	} else {
//		m_rnTopic->sendInterestJoin();
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &RendezvousDrn::sendInterestJoin, this);
	}
}

void
RendezvousDrn::StopApplication() {
	NS_LOG_DEBUG("StopApplication");
//	m_rnTopic->stop();
	if(m_drnPrefix.equals(m_predecessor) == false) {
		m_rnDht->sendInterestLeave();
	}

	m_rnDht.reset();

	App::StopApplication();
}

void
RendezvousDrn::sendInterestJoin() {
	m_rnDht->sendInterestJoin();
}

void
RendezvousDrn::sendDataTM(const Name &interestName, int32_t attentionIndex) {
	// /RN-{yyy}/TM/a/b/c/topic-{nnn}
	// /RN/TM/a/b/c/topic-{nnn}
	// /RN-{yyy}/TM/a/b/*
	// /RN/TM/a/b/*

	Name topicName = interestName.getSubName(attentionIndex, Name::npos);
	string topic = topicName.toUri();

	std::stringstream sio;
	StringMapIterator mapIter;

	mapIter = m_PAMap.begin();
	for( mapIter = m_PAMap.begin(); mapIter != m_PAMap.end(); mapIter ++) {
		NS_LOG_DEBUG(mapIter->first << ": " << mapIter->second);
	}

	// content format: {topic}:{node name},{topic}:{node name},...
	// ex) /a/b/c:RN-00001,/a/b/d:RN-00002

	int at = topic.find('*');
	if(at < 0) {
		mapIter = m_PAMap.find(topic);
		if(mapIter != m_PAMap.end()) {
			sio << stringf("%s:%s", mapIter->first.c_str(), mapIter->second.c_str());
		}
	} else {
		string exp(topic);
		exp.insert(at, 1, '.');
		int count = 0;
		std::regex reg(exp);
		for( mapIter = m_PAMap.begin(); mapIter != m_PAMap.end(); mapIter ++) {
			if(std::regex_match(mapIter->first, reg)) {
				if(0 < count) {
					sio << ',';
				}
				sio << stringf("%s:%s", mapIter->first.c_str(), mapIter->second.c_str());
				count ++;
			}
		}
	}
	string nodes = sio.str();

	Name dataName(interestName);
	// generate data pacaket
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)nodes.c_str(), nodes.size());
	data->setContent(buffer);

#if  1
	Signature signature;
	SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

	if (m_keyLocator.size() > 0) {
		signatureInfo.setKeyLocator(m_keyLocator);
	}

	signature.setInfo(signatureInfo);
	signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

	data->setSignature(signature);
#else
	ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

	// to create real wire encoding
	data->wireEncode();

	// data sent time
	m_appLink->onReceiveData(*data);

	return;
}

void
RendezvousDrn::sendDataPA(const Name &interestName, int32_t attentionIndex) {
	// send Data
	Name dataName(interestName);
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

#if  1
	Signature signature;
	SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

	if (m_keyLocator.size() > 0) {
		signatureInfo.setKeyLocator(m_keyLocator);
	}

	signature.setInfo(signatureInfo);
	signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

	data->setSignature(signature);
#else
	ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

	// to create real wire encoding
	data->wireEncode();

	// data sent time
	m_appLink->onReceiveData(*data);
}

void
RendezvousDrn::OnInterestRN(shared_ptr<const Interest> interest) {
	const Name &interestName = interest->getName();
	int32_t prefixSize = m_rnPrefix.size();
	Name prefix = interestName.getSubName(0, prefixSize);

	if(prefix.equals(m_rnPrefix) == false) {
		return;
	}

	string cmd = interestName.get(prefixSize).toUri();
	if(cmd.compare("PA") == 0) {
		// Publish Advertisement
		//  /RN/PA/a/b/c/topic-{nnn}

		int32_t attentionIndex = prefixSize + 1;

		// /RN/PA/a/b/c/topic-{nnn}에서 3번째를 추출("a")
		name::Component topicPrefix = interestName.get(attentionIndex);
		string topic = topicPrefix.toUri();

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/a/b/c/topic-{nnn}[RN-xxx] Interest를 보낸다.
		m_rnDht->receiveInterestPA(interestName, attentionIndex, topic);

		// send Data
		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
	} else	if(cmd.compare("PU") == 0) {
		// Publish Unadvertisement
		// /RN/PU/a/b/c/topic-{nnn}

		int32_t attentionIndex = prefixSize + 1;
		string topic = interestName.get(attentionIndex).toUri();

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.
		m_rnDht->receiveInterestPU(interestName, topic);

		// send Data
		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);

	} else	if(cmd.compare("DP") == 0) {
		// Data Publish
		// /RN/DP/a/b/c/topic-80/0

		int32_t attentionIndex = prefixSize + 1;
		Name qualifiedName = interestName.getSubName(attentionIndex, interestName.size()-1);
		NS_LOG_DEBUG("qualifiedName: " << qualifiedName);
		Name topicName = qualifiedName.getSubName(0, qualifiedName.size()-1);
		string topic = topicName.toUri();

		const Block &params = interest->getParameters();
		if(0 < params.elements_size()) {
			params.parse();
		}
/*
		const Block &topicBLock = params.get(128);
		const Block &nameBlock = params.get(129);
		string topic((const char *)topicBLock.value(), topicBLock.value_size());
		string nodeName((const char *)nameBlock.value(), nameBlock.value_size());
*/
//		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)successor.first.c_str(), successor.first.size());
		shared_ptr<::ndn::Buffer> buffer = make_shared<::ndn::Buffer>();

		BufferListPtr dataListPtr;
		BufferListMapIterator dataListPtrIter = m_DPMap.find(topic);
		if(dataListPtrIter == m_DPMap.end()) {
			dataListPtr = make_shared<std::vector<BufferPtr>>();
			m_DPMap.insert(pair<string , BufferListPtr>(topic, dataListPtr));
		} else {
			dataListPtr = dataListPtrIter->second;
		}
		dataListPtr->push_back(buffer);

		// send Data
		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);

	} else	if(cmd.compare("TS") == 0) {
		// Topic Subscription
		// /RN/TS/a/b/*
		// /RN/TS/a/b/c/topic-80/*

		int32_t attentionIndex = prefixSize + 1;

		m_rnDht->receiveInterestRNTS(interestName, attentionIndex);

		// send Data
		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
	} else	if(cmd.compare("TM") == 0) {
		// Topic Manifest Request
		// Topic Subscription
		// /RN/TM/a/b/*
		// /RN/TM/a/b/c/topic-80/*
		// /RN/TM/a/b/c/topic-80/0

		NS_LOG_DEBUG("TM: " << interestName);

		int32_t attentionIndex = prefixSize + 1;

		m_rnDht->receiveInterestTM(interestName, attentionIndex);
	}
}

void
RendezvousDrn::OnInterestRNXXX(shared_ptr<const Interest> interest) {
	const Name &interestName = interest->getName();
	int32_t prefixSize = m_drnPrefix.size();

	int32_t cmdIndex = prefixSize;
	string cmd = interestName.get(cmdIndex).toUri();
	if(cmd.compare("join") == 0) {
		// RN-{yyy}/join/join/RN-{xxx}
		int32_t cmd2ndIndex = prefixSize + 1;
		string cmd2nd = interestName.get(cmd2ndIndex).toUri();
		if(cmd2nd.compare("successor") == 0) {
			m_rnDht->receiveInterestJoinSuccessor(interest, cmd2ndIndex);
		} else if(cmd2nd.compare("finger") == 0) {
			m_rnDht->receiveInterestJoinFinger(interest, cmd2ndIndex);
		} else if(cmd2nd.compare("getKeys") == 0) {
			m_rnDht->receiveInterestJoinGetKeys(interest, cmd2ndIndex);
		}
	} else if(cmd.compare("stabilize") == 0) {
		int32_t cmd2ndIndex = cmdIndex + 1;
		string cmd2nd = interestName.get(cmd2ndIndex).toUri();
		if(cmd2nd.compare("checkPredecessor") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				string alive("true");
				shared_ptr<const ::ndn::Buffer> buffer;
				buffer = make_shared<::ndn::Buffer>((const void*)alive.c_str(), alive.size());
				m_rnDht->sendData(interest, buffer);
			}
		} else if(cmd2nd.compare("checkSuccessor") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				string alive("true");
				shared_ptr<const ::ndn::Buffer> buffer;
				buffer = make_shared<::ndn::Buffer>((const void*)alive.c_str(), alive.size());
				m_rnDht->sendData(interest, buffer);
			} else if(cmd3th.compare("sendSuccList") == 0) {
				m_rnDht->receiveInterestSendSuccessorList(interest, cmd2ndIndex);
			}
		} else if(cmd2nd.compare("alive") == 0) {
			string alive("true");
			shared_ptr<const ::ndn::Buffer> buffer;
			buffer = make_shared<::ndn::Buffer>((const void*)alive.c_str(), alive.size());
			m_rnDht->sendData(interest, buffer);
		} else if(cmd2nd.compare("p1") == 0) {
			m_rnDht->receiveInterestStabilizeP1(interest, cmd2ndIndex);
		} else if(cmd2nd.compare("stabilize") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				string alive("true");
				shared_ptr<const ::ndn::Buffer> buffer;
				buffer = make_shared<::ndn::Buffer>((const void*)alive.c_str(), alive.size());
				m_rnDht->sendData(interest, buffer);
			} else if(cmd3th.compare("p1") == 0) {
				m_rnDht->receiveInterestStabilizeP1(interest, cmd3thIndex);
			}
		} else if(cmd2nd.compare("updateSuccessorList") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("sendSuccList") == 0) {
				m_rnDht->receiveInterestSendSuccessorList(interest, cmd3thIndex);
			}
		} else if(cmd2nd.compare("fixFingers") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				string alive("true");
				shared_ptr<const ::ndn::Buffer> buffer;
				buffer = make_shared<::ndn::Buffer>((const void*)alive.c_str(), alive.size());
				m_rnDht->sendData(interest, buffer);
			}
		}
	} else if (cmd.compare("storeKeys") == 0) {
		//
		const Block &params = interest->getParameters();
		string keyAndValues((const char *)params.value(), params.value_size());

		m_rnDht->receiveInterestStoreAllKeys(keyAndValues);

		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);

	} else if (cmd.compare("k") == 0) {
		//
		name::Component keyName = interestName.get(2);
		string value = m_rnDht->receiveInterestGet(keyName.toUri());

		Name dataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)value.c_str(), value.size());
		data->setContent(buffer);

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
	} else if (cmd.compare("sendSuccList") == 0) {
#if 0
		//
		int32_t attentionIndex = prefixSize + 1;
		name::Component keyName = interestName.get(attentionIndex);
		string value = m_rnDht->receiveInterestSendSuccessorList();

		Name dataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)value.c_str(), value.size());
		data->setContent(buffer);

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
#endif
	} else if (cmd.compare("alive") == 0) {
		Name dataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		//
		string value("1");
		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)value.c_str(), value.size());
		data->setContent(buffer);

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
	} else if (cmd.compare("put") == 0) {
		const Block &params = interest->getParameters();
		if(0 < params.elements_size()) {
			params.parse();
		}

		const Block &keyBlock = params.get(127);
		const Block &valueBlock = params.get(128);

		string key((const char *)keyBlock.value(), keyBlock.size());
		lli keyHash = std::stoll(key);

		string value((const char *)valueBlock.value(), valueBlock.size());

		m_rnDht->receiveInterestPut(keyHash, value);
	} else if (cmd.compare("p2") == 0) {
		NS_LOG_DEBUG("recv interest: " << interestName);

		m_rnDht->receiveInterestP2(interest);
	} else if (cmd.compare("PA") == 0) {
		// Publish Advertisement
		//  /RN/PA/topic-{nnn}?RN-{yyy}
		NS_LOG_DEBUG("recv interest: " << interestName);

		int32_t attentionIndex = prefixSize + 1;

		const Block &params = interest->getParameters();
		if(0 < params.elements_size()) {
			params.parse();
		}
#if 0
		const Block &topicBLock = params.get(128);
		const Block &nameBlock = params.get(129);
		string topic((const char *)topicBLock.value(), topicBLock.value_size());
		string nodeName((const char *)nameBlock.value(), nameBlock.value_size());
#else
		string nodeName((const char *)params.value(), params.value_size());
#endif

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);

		m_PAMap.insert(std::pair<std::string, std::string>(topicName.toUri(), nodeName));

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.

//		name::Component topicName = interestName.get(2);
//		string topic = topicName.toUri();
//		lli keyHash = Helper::getHash(topic);
//		m_rnTopic->receiveInterestPATopic(keyHash, nodeName);
	} else if (cmd.compare("PU") == 0) {
		// Publish Unadvertisement
		//  /RN/PU/topic-{nnn}
		NS_LOG_DEBUG("recv interest: " << interestName);

		int32_t attentionIndex = prefixSize + 1;

		const Block &params = interest->getParameters();
		if(0 < params.elements_size()) {
			params.parse();
		}
#if 0
		const Block &topicBLock = params.get(128);
		const Block &nameBlock = params.get(129);
		string topic((const char *)topicBLock.value(), topicBLock.value_size());
		string nodeName((const char *)nameBlock.value(), nameBlock.value_size());
#else
		string nodeName((const char *)params.value(), params.value_size());
#endif

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);

		std::map<std::string, std::string>::iterator iter;
		iter = m_PAMap.find(topicName.toUri());
		if(iter != m_PAMap.end()) {
			m_PAMap.erase(iter);
		}

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.
//		m_rnTopic->receiveInterestPUTopic(keyHash);
	} else if (cmd.compare("TS") == 0) {
		NS_LOG_DEBUG("recv interest: " << interestName);
		//
		int32_t attentionIndex = prefixSize + 1;

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		string topic = topicName.toUri();

		std::stringstream sio;
		StringMapIterator mapIter;
		int at = topic.find('*');
		if(at < 0) {
			mapIter = m_PAMap.find(topic);
			if(mapIter != m_PAMap.end()) {
				sio << mapIter->second;
			}
		} else {
			string exp(topic);
			exp.insert(at, 1, '.');
			int count = 0;
			std::regex reg(exp);
			for( mapIter = m_PAMap.begin(); mapIter != m_PAMap.end(); mapIter ++) {
				if(std::regex_match(mapIter->first, reg)) {
					if(0 < count) {
						sio << ':';
					}
					sio << mapIter->second;
					count ++;
				}
			}
		}
		string nodes = sio.str();
#if 0
		string value = m_rnDht->receiveInterestSendSuccessorList();
#endif
		Name dataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)nodes.c_str(), nodes.size());
		data->setContent(buffer);

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
	} else if (cmd.compare("TS") == 0) {
		NS_LOG_DEBUG("recv interest: " << interestName);
		//
		int32_t attentionIndex = prefixSize + 1;

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		string topic = topicName.toUri();

	} else if (cmd.compare("TM") == 0) {
		NS_LOG_DEBUG("recv interest: " << interestName);
		//
		int32_t attentionIndex = prefixSize + 1;
		sendDataTM(interestName, attentionIndex);
		return;
		// string value = m_rnDht->receiveInterestSendSuccessorList();
	} else	if(cmd.compare("DM") == 0) {
		NS_LOG_DEBUG("recv interest: " << interestName);
		// Data Manifest Request
		int32_t attentionIndex = prefixSize + 1;

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		string topic = topicName.toUri();

		size_t topicSize = topicName.size();

		::ndn::Block content(::ndn::tlv::Content);
		BufferListMapIterator mapIter = m_DPMap.begin();
		for(; mapIter != m_DPMap.end(); mapIter ++) {
			// /a/b/c/topic-0/0
			Name publishTopicName(mapIter->first);
			if(publishTopicName.size() < topicSize) {
				continue;
			}

			if(publishTopicName.getPrefix(prefixSize).equals(topicName) == false) {
				continue;
			}
			::ndn::Block nameBlock(::ndn::tlv::Name, Block((uint8_t *)mapIter->first.c_str(), mapIter->first.size()));
			content.push_back(nameBlock);
		}

		Name tmDataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(tmDataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		data->setContent(content);

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);
	} else	if(cmd.compare("DR") == 0) {
		NS_LOG_DEBUG("recv interest: " << interestName);
		// Data Request
		// Data Manifest Request
		int32_t attentionIndex = prefixSize + 1;

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		string topic = topicName.toUri();

		Name tmDataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(tmDataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		BufferListMapIterator mapIter = m_DPMap.find(topic);
		if(mapIter != m_DPMap.end()) {
			// /a/b/c/topic-0/0
			::ndn::Block content(::ndn::tlv::Content);

			// std::vector<std::shared_ptr<ndn::Buffer> >::iterator

			std::shared_ptr<std::vector<BufferPtr>> second = mapIter->second;

			BufferListPtr dataListPtr = mapIter->second;
			BufferListIterator iter = dataListPtr->begin();
			for(int i = 0; iter != dataListPtr->end(); iter ++, i++) {
				::ndn::Block data(::ndn::tlv::AppPrivateBlock1+i, *iter);
				content.push_back(data);
			}
			data->setContent(content);
		}

#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);

	}
}

void
RendezvousDrn::OnInterest(shared_ptr<const Interest> interest) {
	const Name &interestName = interest->getName();
	const name::Component &topPrefix = interest->getName().get(0);
	string prefix = topPrefix.toUri();

	NS_LOG_DEBUG("recv interest: " << interestName);

	if(prefix == m_rnPrefix) {
		OnInterestRN(interest);
	} else if(prefix == m_drnPrefix) {
		OnInterestRNXXX(interest);
	}
}

void
RendezvousDrn::OnDataRN(shared_ptr<const Data> data) {
	name::Component nodeName = data->getName().get(1);
	name::Component cmdName = data->getName().get(2);
	string cmd = cmdName.toUri();
}

void
RendezvousDrn::OnDataRNXXX(shared_ptr<const Data> data) {
	const Name &dataName = data->getName();

	// /RN/PA/..., /RN/PU/..., /RN/DP/..., /RN/TS/..., /RN/TM/...,
	// /RN-{yyyyy}/PA/..., /RN-{yyyyy}/PU/..., /RN-{yyyyy}/DP/..., /RN-{yyyyy}/TS/..., /RN-{yyyyy}/TM/...,
	int32_t prefixSize = 1;
	int32_t cmdIndex = prefixSize;

	string cmd = dataName.get(cmdIndex).toUri();
	if(cmd.compare("join") == 0) {
		int32_t cmd2ndIndex = cmdIndex + 1;
		string cmd2nd = dataName.get(cmd2ndIndex).toUri();
		if(cmd2nd.compare("successor") == 0) {
			m_rnDht->receiveDataJoinSuccessor(data, cmd2ndIndex);
		} else if(cmd2nd.compare("finger") == 0) {
			m_rnDht->receiveDataJoinFinger(data, cmd2ndIndex);
		} else if(cmd2nd.compare("getKeys") == 0) {
			m_rnDht->receiveDataJoinGetKeys(data, cmd2ndIndex);
		}
	} else if(cmd.compare("stabilize") == 0) {
		int32_t cmd2ndIndex = prefixSize + 1;
		string cmd2nd = dataName.get(cmd2ndIndex).toUri();
		if(cmd2nd.compare("checkPredecessor") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = dataName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				m_rnDht->receiveDataPredecessorAlive(data, cmd2ndIndex);
			}
		} else if(cmd2nd.compare("checkSuccessor") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = dataName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				m_rnDht->receiveDataSuccessorAlive(data, cmd3thIndex);
			} else if(cmd3th.compare("sendSuccList") == 0) {
				m_rnDht->receiveDataSendSuccessorList(data, cmd3thIndex);
			}
		} else if(cmd2nd.compare("stabilize") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = dataName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				m_rnDht->receiveDataStabilizeAlive(data, cmd3thIndex);
			} else if(cmd3th.compare("p1") == 0) {
				m_rnDht->receiveDataStabilizeP1(data, cmd3thIndex);
			}
		} else if(cmd2nd.compare("p1") == 0) {
			int32_t nodeIndex = cmd2ndIndex + 1;
			string nodeName = dataName.get(nodeIndex).toUri();
		} else if(cmd2nd.compare("updateSuccessorList") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = dataName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("sendSuccList") == 0) {
				m_rnDht->receiveInterestUpdateSuccessorList(data, cmd3thIndex);
			}
		} else if(cmd2nd.compare("fixFingers") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = dataName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				m_rnDht->receiveDataFixFingersAlive(data, cmd3thIndex);
			}
		}
	} else if(cmd.compare("sendSuccList") == 0) {
#if 0
		// TODO: SuccList는 ;를 구분자로 문자열로 넘어온다.
		const Block &content = data->getContent();
		string successorList((const char *)content.value(), content.value_size());

		NS_LOG_DEBUG("contents: successorList: " << successorList);

		m_rnDht->receiveDataSendSuccessorList(successorList);
#endif
	} else if(cmd.compare("storeKeys") == 0) {
		// TODO: storeKeys 명령을 실행한 결과를 처리한다.
	} else if(cmd.compare("k") == 0) {
		int32_t attentionIndex = prefixSize + 1;
		string key = data->getName().get(attentionIndex).toUri();

		const Block &content = data->getContent();
		string value((const char *)content.value(), content.value_size());

		NS_LOG_DEBUG("contents: value: " << value);

		m_rnDht->receiveDataGet(key, value);
	} else if(cmd.compare("alive") == 0) {
		const Block &content = data->getContent();
		string isAlive((const char *)content.value(), content.value_size());

		NS_LOG_DEBUG("contents: isAlive: " << isAlive);

		Name keyName = data->getName().getPrefix(1);

		m_rnDht->receiveDataAlive(keyName.toUri(), isAlive);
	} else	if(cmd.compare("TS") == 0) {
		NS_LOG_DEBUG("recv data: " << dataName);
		// /RN-{yyyyy}/TS/...에  대한 응답(Data)
		int32_t attentionIndex = prefixSize + 1;
		Name topicName = dataName.getSubName(attentionIndex, Name::npos);

	} else if(cmd.compare("TM") == 0) {
		NS_LOG_DEBUG("recv data: " << dataName);
		// /RN-{yyyyy}/TM/...에  대한 응답(Data)
		int32_t attentionIndex = prefixSize + 1;
		Name topicName = dataName.getSubName(attentionIndex, Name::npos);

		Name tmpName("RN");
		tmpName.append(cmd);
		tmpName.append(topicName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(dataName);
		// data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		const Block &content = data->getContent();
		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)content.value(), content.value_size());
		data->setContent(buffer);

		NS_LOG_DEBUG(dataName << " : " << string((const char *)content.value(), content.value_size()));

	#if  1
		Signature signature;
		SignatureInfo signatureInfo(static_cast<::ndn::tlv::SignatureTypeValue>(255));

		if (m_keyLocator.size() > 0) {
			signatureInfo.setKeyLocator(m_keyLocator);
		}

		signature.setInfo(signatureInfo);
		signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

		data->setSignature(signature);
	#else
		ns3::ndn::StackHelper::getKeyChain().sign(*data);
	#endif

		// to create real wire encoding
		data->wireEncode();

		// data sent time
		m_appLink->onReceiveData(*data);

	}
}

void
RendezvousDrn::OnData(shared_ptr<const Data> data) {
	const Name &dataName = data->getName();

//	NS_LOG_DEBUG("recv data: " << dataName);

	Name prefixName = dataName.getPrefix(1);
	Name cmdName;
	cmdName.append(dataName.get(1));

//	NS_LOG_DEBUG("join (data)-prefixName: " << prefixName);
//	NS_LOG_DEBUG("join (data)-cmdName: " << cmdName);
//	NS_LOG_DEBUG("join (data)-m_rnPrefix: " << m_rnPrefix);
//	NS_LOG_DEBUG("join (data)-m_drnPrefix: " << m_drnPrefix);

#if 0
	if(prefixName.equals(m_rnPrefix)) {
		OnDataRN(data);
	} else if(prefixName.equals(m_drnPrefix)) {
		OnDataRNXXX(data);
	}
#else
	OnDataRNXXX(data);
#endif
}

void
RendezvousDrn::OnNack(shared_ptr<const ::ndn::lp::Nack> nack) {
	const Interest &interest = nack->getInterest();
	const Name &interestName = interest.getName();
	int32_t prefixSize = 1;

	int32_t cmdIndex = prefixSize;
	string cmd = interestName.get(cmdIndex).toUri();
	if(cmd.compare("stabilize") == 0) {
		int32_t cmd2ndIndex = cmdIndex + 1;
		string cmd2nd = interestName.get(cmd2ndIndex).toUri();
		if(cmd2nd.compare("predecessor") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				// /RN-yyyyy/stabilize/checkPredecessor/alive
				m_rnDht->setDeadPredecessor();
			}
		} else if(cmd2nd.compare("successor") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				// /RN-yyyyy/stabilize/checkSuccessor/alive
				m_rnDht->sendInterestCheckSuccessorUpdateSuccessorList();
			}
		} else if(cmd2nd.compare("stabilize") == 0) {
			int32_t cmd3thIndex = cmd2ndIndex + 1;
			string cmd3th = interestName.get(cmd3thIndex).toUri();
			if(cmd3th.compare("alive") == 0) {
				// /RN-yyyyy/stabilize/stabilize/alive
				int step = 3;
			#if 0
				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
			#else
				m_rnDht->doStabilize(step);
			#endif
			}
		}
	}
}
