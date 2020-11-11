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
#include <ns3/ndnSIM/ndn-cxx/encoding/block.hpp>
#include <ns3/ndnSIM/ndn-cxx/encoding/buffer.hpp>
#include <ns3/ndnSIM/ndn-cxx/util/string-helper.hpp>

//#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include "ndn-consumer-drn.hpp"
#include "ndn-rendezvous-drn-f.hpp"

#include "utils.hpp"

using namespace ns3::ndn;

NS_LOG_COMPONENT_DEFINE("drn.RendezvousDrnF");

NS_OBJECT_ENSURE_REGISTERED(RendezvousDrnF);

ns3::TypeId
RendezvousDrnF::GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("RendezvousDrnF")
        .SetGroupName("Ndn")
		.SetParent<Application>()
		.AddConstructor<RendezvousDrnF>()

		.AddAttribute("RnPrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
		                MakeNameAccessor(&RendezvousDrnF::m_rnPrefix), MakeNameChecker())

		.AddAttribute("DRnPrefix", "Prefix, for which producer has the data", ns3::StringValue("/"),
						MakeNameAccessor(&RendezvousDrnF::m_drnPrefix), MakeNameChecker())

		.AddAttribute("NumSubscribeMessage", "Number of subscribe messages", ns3::UintegerValue(100),
						ns3::MakeUintegerAccessor(&RendezvousDrnF::m_nSub), ns3::MakeUintegerChecker<uint32_t>())

		.AddAttribute("DataSize", "Size of messages", ns3::UintegerValue(100),
						ns3::MakeUintegerAccessor(&RendezvousDrnF::m_nDataSize), 
                        ns3::MakeUintegerChecker<uint32_t>())

		.AddAttribute("CustomAttributes", "Custom Attributes", PointerValue (),
						ns3::MakePointerAccessor(&RendezvousDrnF::m_objectContainer), 
                        ns3::MakePointerChecker<ObjectContainer>())

		.AddAttribute("LifeTime", "LifeTime for interest packet", StringValue("4s"),
						MakeTimeAccessor(&RendezvousDrnF::m_interestLifeTime), MakeTimeChecker())
						;
    return tid;
}

RendezvousDrnF::RendezvousDrnF()
	: m_nSub(0)
	, m_nDataSize(0)
	, m_rng(::ndn::random::getRandomNumberEngine())
	, m_appId(std::numeric_limits<uint32_t>::max())
	, m_signature(0U)
	, m_nRN__TSInterestCount(0)
	, m_nRNsnTSInterestCount(0)
	, m_nRNrvTSInterestCount(0)
	, m_nRN__TMInterestCount(0)
	, m_nRNsnTMInterestCount(0)
	, m_nRNrvTMInterestCount(0)
	, m_nRNrvDMInterestCount(0)
	, m_nRNrvDRInterestCount(0)

	, m_nRNsnTSNackCount(0)
	, m_nRNsnTMNackCount(0)

	, m_nRN__TSDataCount(0)
	, m_nRNsnTSDataCount(0)
	, m_nRNrvTSDataCount(0)
	, m_nRN__TMDataCount(0)
	, m_nRNsnTMDataCount(0)
	, m_nRNrvTMDataCount(0)
	, m_nRNsnDMDataCount(0)
	, m_nRNsnDRDataCount(0)

	, m_nRN__PAInterestCount(0)
	, m_nRNsnPAInterestCount(0)
	, m_nRNrvPAInterestCount(0)
	, m_nRN__PUInterestCount(0)
	, m_nRNsnPUInterestCount(0)
	, m_nRNrvPUInterestCount(0)
	, m_nRN__DPInterestCount(0)

	, m_nRNsnPANackCount(0)
	, m_nRNsnPUNackCount(0)

	, m_nRN__PADataCount(0)
	, m_nRNsnPADataCount(0)
	, m_nRNrvPADataCount(0)
	, m_nRN__PUDataCount(0)
	, m_nRNsnPUDataCount(0)
	, m_nRNrvPUDataCount(0)
	, m_nRN__DPDataCount(0)
{
}

void
RendezvousDrnF::StartApplication() {
	TimeoutApp::StartApplication();

	FibHelper::AddRoute(GetNode(), m_drnPrefix, m_face, 0);
	FibHelper::AddRoute(GetNode(), m_rnPrefix, m_face, 0);
	NS_LOG_DEBUG(stringf("AddFIB(%5u): ", GetNode()->GetId()) << m_drnPrefix);

	m_objectContainer->get("dht-nodes", m_dhtNodes);

#if 0
	std::vector<std::string>::iterator iter = m_dhtNodes->begin();
	for ( ; iter != m_dhtNodes->end(); iter ++) {
		NS_LOG_DEBUG(*iter);
	}
#endif
}

void
RendezvousDrnF::StopApplication() {
	NS_LOG_DEBUG("StopApplication");
    //m_rnTopic->stop();

	TimeoutApp::StopApplication();
	NS_LOG_INFO(stringf("Recv RN   TSInterestCount: %5u", m_nRN__TSInterestCount));
	NS_LOG_INFO(stringf("Send RNxx TSInterestCount: %5u", m_nRNsnTSInterestCount));
	NS_LOG_INFO(stringf("Recv RNxx TSInterestCount: %5u", m_nRNrvTSInterestCount));
	NS_LOG_INFO(stringf("Recv RN   TMInterestCount: %5u", m_nRN__TMInterestCount));
	NS_LOG_INFO(stringf("Send RNxx TMInterestCount: %5u", m_nRNsnTMInterestCount));
	NS_LOG_INFO(stringf("Recv RNxx TMInterestCount: %5u", m_nRNrvTMInterestCount));
	NS_LOG_INFO(stringf("Recv RNxx DMInterestCount: %5u", m_nRNrvDMInterestCount));
	NS_LOG_INFO(stringf("Recv RNxx DRInterestCount: %5u", m_nRNrvDRInterestCount));

	NS_LOG_INFO(stringf("Nack RNxx TSInterestCount: %5u", m_nRNsnTSNackCount));
	NS_LOG_INFO(stringf("Nack RNxx TMInterestCount: %5u", m_nRNsnTMNackCount));

	NS_LOG_INFO(stringf("Recv RN   TSDataCount:     %5u", m_nRN__TSDataCount));
	NS_LOG_INFO(stringf("Send RNxx TSDataCount:     %5u", m_nRNsnTSDataCount));
	NS_LOG_INFO(stringf("Recv RNxx TSDataCount:     %5u", m_nRNrvTSDataCount));
	NS_LOG_INFO(stringf("Recv RN   TMDataCount:     %5u", m_nRN__TMDataCount));
	NS_LOG_INFO(stringf("Send RNxx TMDataCount:     %5u", m_nRNsnTMDataCount));
	NS_LOG_INFO(stringf("Recv RNxx TMDataCount:     %5u", m_nRNrvTMDataCount));
	NS_LOG_INFO(stringf("Send RNxx DMDataCount:     %5u", m_nRNsnDMDataCount));
	NS_LOG_INFO(stringf("Send RNxx DRDataCount:     %5u", m_nRNsnDRDataCount));

	NS_LOG_INFO(stringf("Recv RN   PAInterestCount: %5u", m_nRN__PAInterestCount));
	NS_LOG_INFO(stringf("Send RNxx PAInterestCount: %5u", m_nRNsnPAInterestCount));
	NS_LOG_INFO(stringf("Recv RNxx PAInterestCount: %5u", m_nRNrvPAInterestCount));
	NS_LOG_INFO(stringf("Recv RN   PUInterestCount: %5u", m_nRN__PUInterestCount));
	NS_LOG_INFO(stringf("Send RNxx PUInterestCount: %5u", m_nRNsnPUInterestCount));
	NS_LOG_INFO(stringf("Recv RNxx PUInterestCount: %5u", m_nRNrvPUInterestCount));
	NS_LOG_INFO(stringf("Recv RN   DPInterestCount: %5u", m_nRN__DPInterestCount));

	NS_LOG_INFO(stringf("Nack RNxx PAInterestCount: %5u", m_nRNsnPANackCount));
	NS_LOG_INFO(stringf("Nack RNxx PUInterestCount: %5u", m_nRNsnPUNackCount));

	NS_LOG_INFO(stringf("Recv RN   PADataCount: %5u", m_nRN__PADataCount));
	NS_LOG_INFO(stringf("Send RNxx PADataCount: %5u", m_nRNsnPADataCount));

	NS_LOG_INFO(stringf("Recv RNxx PADataCount:     %5u", m_nRNrvPADataCount));
	NS_LOG_INFO(stringf("Recv RN   PUDataCount:     %5u", m_nRN__PUDataCount));
	NS_LOG_INFO(stringf("Send RNxx PUDataCount:     %5u", m_nRNsnPUDataCount));
	NS_LOG_INFO(stringf("Recv RNxx PUDataCount:     %5u", m_nRNrvPUDataCount));
	NS_LOG_INFO(stringf("Recv RN   DPDataCount:     %5u", m_nRN__DPDataCount));
}

void
RendezvousDrnF::sendData(const Name &dataName, const Block &content) {
	auto data = make_shared<Data>();
	data->setName(dataName);
    //data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	if (0 < content.value_size() || 0 < content.elements_size()) {
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
	removePendingTimeoutEvent(data->getName());
}

void
RendezvousDrnF::sendData(shared_ptr<const Interest> interest, const Block &content) {
	Name dataName(interest->getName());
	sendData(dataName, content);
}

void
RendezvousDrnF::sendData(const Name &dataName, shared_ptr<const ::ndn::Buffer> &value) {
	auto data = make_shared<Data>();
	data->setName(dataName);
    //data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	if (0 < value->size()) {
		data->setContent(value);
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

void
RendezvousDrnF::sendData(shared_ptr<const Interest> interest, shared_ptr<const ::ndn::Buffer> &value) {
	Name dataName(interest->getName());
	sendData(dataName, value);
}

int
RendezvousDrnF::sendDataTM(const Name &interestName, int32_t attentionIndex) {
	// /RN-{yyy}/TM/a/b/c/topic-{nnn}
	// /RN/TM/a/b/c/topic-{nnn}
	// /RN-{yyy}/TM/a/b/*
	// /RN/TM/a/b/*

	Name topicName = interestName.getSubName(attentionIndex, Name::npos);
	string topic = ::ndn::unescape(topicName.toUri());

	std::stringstream sio;
	StringMapIterator mapIter;

	mapIter = m_PAMap.begin();
	for ( mapIter = m_PAMap.begin(); mapIter != m_PAMap.end(); mapIter ++) {
		NS_LOG_DEBUG(mapIter->first << ": " << mapIter->second);
	}

	// content format: {topic}:{node name},{topic}:{node name},...
	// ex) /a/b/c:RN-00001,/a/b/d:RN-00002

	int at = topic.find('*');
	if (at < 0) {
		mapIter = m_PAMap.find(topic);
		if (mapIter != m_PAMap.end()) {
			sio << stringf("%s:%s", mapIter->first.c_str(), mapIter->second.c_str());
		}
	} else {
		string exp(topic);
		exp.insert(at, 1, '.');
		int count = 0;
		std::regex reg(exp);
		for ( mapIter = m_PAMap.begin(); mapIter != m_PAMap.end(); mapIter ++) {
			if (std::regex_match(mapIter->first, reg)) {
				if (0 < count) {
					sio << ',';
				}
				sio << stringf("%s:%s", mapIter->first.c_str(), mapIter->second.c_str());
				count ++;
			}
		}
	}
	string nodes = sio.str();
	if(nodes.size() == 0) {
		return 0;
	}

	shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)nodes.c_str(), nodes.size());

	Name dataName(interestName);
	sendData(dataName, buffer);

	m_nRNsnTMDataCount += 1;

	return 1;
}

void
RendezvousDrnF::sendDataPA(const Name &interestName, int32_t attentionIndex) {
	// send Data
	Name dataName(interestName);
	auto data = make_shared<Data>();
	data->setName(dataName);
    //data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

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
	removePendingTimeoutEvent(data->getName());
}

void 
RendezvousDrnF::receiveInterestRNPA(const Name &interestName, int32_t attentionIndex, string topic) {
	//  topic의 hash값을 계산하고
	lli topicHash = getHash(topic);

	// 담당 Node를  찾는다.
	int index = topicHash % m_dhtNodes->size();
	string nodeName = m_dhtNodes->at(index);

	NS_LOG_DEBUG("TopicRN: " << nodeName);

	if (m_drnPrefix.equals(Name(nodeName))) {
		// self
		Name topicName = interestName.getSubName(attentionIndex, Name::npos);

		m_PAMap.insert(std::pair<std::string, std::string>(topicName.toUri(), nodeName));
		return;
	}

	sendInterestRNXXXPA(interestName, attentionIndex, nodeName);
}

void 
RendezvousDrnF::sendInterestRNXXXPA(const Name &interestName, int32_t attentionIndex, string nodeName) {
	// 찾으면
	// /RN-{yyy}/PA/a/b/c/topic-{nnn}
	Name paInterestName(nodeName);
	paInterestName.append("PA");

	Name topicName = interestName.getSubName(attentionIndex, Name::npos);
	paInterestName.append(topicName);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(paInterestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
    //time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    //interest->setInterestLifetime(interestLifeTime);

	shared_ptr<const ::ndn::Buffer> buffer;
#if 0
	// /a/b/c/topic-{nnn}에 대한 담당 RN-{xxx}를 전달
	buffer = make_shared<::ndn::Buffer>((const void*)topic.c_str(), topic.size());
	ndn::Block keyBlock(128, buffer); // key

	string drnPrefix = m_drnPrefix.get(0).toUri();
	buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
	ndn::Block valueBlock(129, buffer); // value

	ndn::Block params(ndn::tlv::Parameters);
	params.push_back(keyBlock);
	params.push_back(valueBlock);
#else
	string drnPrefix = m_drnPrefix.get(0).toUri();
	buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
	::ndn::Block params(::ndn::tlv::Parameters, buffer);
#endif
	// 여러개는 보내기 전에 encoding을 해야 하고, 받아서는 parse를 해야 한다.
	params.encode();

	interest->setParameters(params);

	NS_LOG_DEBUG("send interest: " << paInterestName);

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nRNsnPAInterestCount += 1;
}

void 
RendezvousDrnF::receiveInterestRNPU(const Name &interestName, string topic) {
	// topic의 hash값을 계산하고
	lli topicHash = getHash(topic);

	// 담당 Node를  찾는다.
	int index = topicHash % m_dhtNodes->size();
	string nodeName = m_dhtNodes->at(index);

	NS_LOG_DEBUG("TopicRN: " << nodeName);

	if (m_drnPrefix.equals(Name(nodeName))) {
		// self
		return;
	}

	// 찾으면
	// /RN-{yyy}/PA/topic-{nnn}/...
	Name puInterestName(nodeName);
	puInterestName.append("PU");

	Name topicName = interestName.getSubName(2, Name::npos);
	puInterestName.append(topicName);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(puInterestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
    //time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    //interest->setInterestLifetime(interestLifeTime);

    //ndn::tlv::AppPrivateBlock1 = 128;
    //ndn::tlv::AppPrivateBlock2 = 32767;
    //ndn::tlv::ContentType_Blob = 0;

	shared_ptr<const ::ndn::Buffer> buffer;

	// /a/b/c/topic-{nnn} : RN-{nnn} 에 대한 un-advertise 메시지를 보내다.
#if 0
	buffer = make_shared<::ndn::Buffer>((const void*)topic.c_str(), topic.size());
	ndn::Block keyBlock(128, buffer); // key
	string drnPrefix = m_drnPrefix.get(0).toUri();
	buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
	ndn::Block valueBlock(129, buffer); // value

	ndn::Block params(ndn::tlv::Parameters);
	params.push_back(keyBlock);
	params.push_back(valueBlock);
#else
	string drnPrefix = m_drnPrefix.get(0).toUri();
	buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
	::ndn::Block params(::ndn::tlv::Parameters, buffer);
#endif
	params.encode();

	interest->setParameters(params);

	NS_LOG_DEBUG("send interest: " << puInterestName << ", topic prefix: " << topic << ", value: " << drnPrefix );

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nRNsnPUInterestCount += 1;

	return;
}

void 
RendezvousDrnF::receiveInterestRNTS(const Name &interestName, int32_t attentionIndex) {

	// /RN/TS/a/b/c/topic-{nnn}에서 3번째를 추출("a")
	string topicPrefix = interestName.get(attentionIndex).toUri();

	// topic의 hash값을 계산하고
	lli topicHash = getHash(topicPrefix);

	// 담당 Node를  찾는다.
	int index = topicHash % m_dhtNodes->size();
	string nodeName = m_dhtNodes->at(index);

	NS_LOG_DEBUG("TopicRN: " << nodeName);

	if (m_drnPrefix.equals(Name(nodeName))) {
		Name dataName(interestName);

		Block empty;
		sendData(dataName, empty);
		return;
	}

	// 찾으면
	// /RN-{yyy}/TS/a/b/c
	Name tsInterestName(nodeName);
	tsInterestName.append("TS");

	Name topicName = interestName.getSubName(attentionIndex, Name::npos);
	tsInterestName.append(topicName);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(tsInterestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
    //time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    //interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << tsInterestName );

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif
	m_nRNsnTSInterestCount += 1;

	return;
}

void 
RendezvousDrnF::receiveInterestRNTM(const Name &interestName, int32_t attentionIndex) {
	// /RN/TS/a/b/c/topic-{nnn}에서 3번째를 추출("a")
	string topicPrefix = interestName.get(attentionIndex).toUri();

	// topic의 hash값을 계산하고
	lli topicHash = getHash(topicPrefix);

	// 담당 Node를  찾는다.
	int index = topicHash % m_dhtNodes->size();
	string nodeName = m_dhtNodes->at(index);

	NS_LOG_DEBUG("TopicRN: " << nodeName);

	if (m_drnPrefix.equals(Name(nodeName))) {
		// self
		if(0 < sendDataTM(interestName, attentionIndex)) {
			removePendingTimeoutEvent(interestName);
		}
		return;
	}

	sendInterestRNXXXTM(interestName, attentionIndex, nodeName);
}

void 
RendezvousDrnF::sendInterestRNXXXTM(const Name &interestName, int32_t attentionIndex, string nodeName) {
	// /RN/TM/a/b/* -> /RN-{xxx}/TM/a/b/*
	// /RN/TM/a/b/c/topic-80/* -> /RN-{xxx}/TM/a/b/c/topic-80/*
	// /RN/TM/a/b/c/topic-80/0 -> /RN-{xxx}/TM/a/b/c/topic-80/0

	Name tmInterestName(nodeName);
	tmInterestName.append("TM");

	Name topicName = interestName.getSubName(attentionIndex, Name::npos);
	tmInterestName.append(topicName);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(tmInterestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
    //time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    //interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << tmInterestName );

#if 1
	m_appLink->onReceiveInterest(*interest);
#else
	sendInterestTimeout(interest);
#endif

	m_nRNsnTMInterestCount += 1;

	return;
}

void
RendezvousDrnF::OnInterestRN(shared_ptr<const Interest> interest) {
	const Name &interestName = interest->getName();
	int32_t prefixSize = m_rnPrefix.size();
	Name prefix = interestName.getSubName(0, prefixSize);

	if (prefix.equals(m_rnPrefix) == false) {
		return;
	}

	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("PA") == 0) {
		m_nRN__PAInterestCount += 1;
		// Publish Advertisement
		// /RN/PA/a/b/c/topic-{nnn}

		int32_t attentionIndex = prefixSize + 1;

		// /RN/PA/a/b/c/topic-{nnn}에서 3번째를 추출("a")
		name::Component topicPrefix = interestName.get(attentionIndex);
		string topic = topicPrefix.toUri();

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/a/b/c/topic-{nnn}[RN-xxx] Interest를 보낸다.
		receiveInterestRNPA(interestName, attentionIndex, topic);

		// send Data
		Name dataName(interestName);

		Block empty;
		sendData(dataName, empty);
		m_nRN__PADataCount += 1;

	} else	if (cmd.compare("PU") == 0) {
		m_nRN__PUInterestCount += 1;

		// Publish Unadvertisement
		// /RN/PU/a/b/c/topic-{nnn}

		int32_t attentionIndex = prefixSize + 1;
		string topic = interestName.get(attentionIndex).toUri();

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.
		receiveInterestRNPU(interestName, topic);

		// send Data
		Name dataName(interestName);

		Block empty;
		sendData(dataName, empty);
		m_nRN__PUDataCount += 1;
	} else	if (cmd.compare("DP") == 0) {
		m_nRN__DPInterestCount += 1;
		// Data Publish
		// /RN/DP/a/b/c/topic-80/0

		int32_t attentionIndex = prefixSize + 1;
		Name qualifiedName = interestName.getSubName(attentionIndex, interestName.size()-1);
		NS_LOG_DEBUG("qualifiedName: " << qualifiedName);
		Name topicName = qualifiedName.getSubName(0, qualifiedName.size()-1);
		string topic = topicName.toUri();
		string seqStr = qualifiedName.get(qualifiedName.size()-1).toUri();

		const Block &params = interest->getParameters();
        /*
		if (0 < params.elements_size()) {
			params.parse();
		}
        */
		params.parse();
        /*
		const Block &topicBLock = params.get(128);
		const Block &nameBlock = params.get(129);
		string topic((const char *)topicBLock.value(), topicBLock.value_size());
		string nodeName((const char *)nameBlock.value(), nameBlock.value_size());
        */

		const Block& paramBlock = params.get(::ndn::tlv::AppPrivateBlock1);
		shared_ptr<::ndn::Buffer> buffer = make_shared<::ndn::Buffer>(paramBlock.value(), paramBlock.value_size());
		NS_LOG_DEBUG("Published data size: " << buffer->size());
        NS_LOG_INFO("RNStoreData: " << qualifiedName);

		// tuple(seq, data) 를 넣어야 하나 여기서는 sequence만 넣는다.
		BufferPtr bufferPtr = make_shared<::ndn::Buffer>((const void*)seqStr.c_str(), seqStr.size());

		BufferListPtr dataListPtr;
		BufferListMapIterator dataListPtrIter = m_DPMap.find(topic);
		if (dataListPtrIter == m_DPMap.end()) {
			dataListPtr = make_shared<std::vector<BufferPtr>>();
			m_DPMap.insert(pair<string , BufferListPtr>(topic, dataListPtr));
		} else {
			dataListPtr = dataListPtrIter->second;
		}
		dataListPtr->push_back(bufferPtr);

		// Pending 된 DM interest가 있으면 Data를 보낸다.
		sendDataForPendingInsterestDM(interestName);

		// send Data for DP
		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
        //data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

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
		m_nRN__DPDataCount += 1;
		removePendingTimeoutEvent(data->getName());
	} else	if (cmd.compare("TS") == 0) {
		m_nRN__TSInterestCount += 1;
		// Topic Subscription
		// /RN/TS/a/b/*
		// /RN/TS/a/b/c/topic-80/*

		int32_t attentionIndex = prefixSize + 1;

		receiveInterestRNTS(interestName, attentionIndex);

		// self
		Name topicName = interestName.getSubName(attentionIndex, Name::npos);

		Name dataName(interestName);

		// send Data
		Block empty;
		sendData(dataName, empty);
		m_nRN__TSDataCount += 1;
	} else	if (cmd.compare("TM") == 0) {
		m_nRN__TMInterestCount += 1;
		// Topic Manifest Request
		// Topic Subscription
		// /RN/TM/a/b/*
		// /RN/TM/a/b/c/topic-80/*
		// /RN/TM/a/b/c/topic-80/0

		NS_LOG_DEBUG("TM: " << interestName);

		int32_t attentionIndex = prefixSize + 1;

		receiveInterestRNTM(interestName, attentionIndex);
	}
}

void
RendezvousDrnF::OnInterestRNXXX(shared_ptr<const Interest> interest) {
	const Name &interestName = interest->getName();
	int32_t prefixSize = m_drnPrefix.size();

	int32_t cmdIndex = prefixSize;
	string cmd = interestName.get(cmdIndex).toUri();
	if (cmd.compare("PA") == 0) {
		m_nRNrvPAInterestCount += 1;
		// Publish Advertisement
		// /RN/PA/topic-{nnn}?RN-{yyy}
		NS_LOG_DEBUG("recv interest: " << interestName);

		int32_t attentionIndex = prefixSize + 1;

		const Block &params = interest->getParameters();
		if (0 < params.elements_size()) {
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

		sendDataForPendingInsterestTM(interestName);

		Name dataName(interestName);

		Block empty;
		sendData(dataName, empty);
		m_nRNsnPADataCount += 1;

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.

        //name::Component topicName = interestName.get(2);
        //string topic = topicName.toUri();
        //lli keyHash = Helper::getHash(topic);
        //m_rnTopic->receiveInterestPATopic(keyHash, nodeName);
		return;
	} else if (cmd.compare("PU") == 0) {
		m_nRNrvPUInterestCount += 1;
		// Publish Unadvertisement
		// /RN/PU/topic-{nnn}
		NS_LOG_DEBUG("recv interest: " << interestName);

		int32_t attentionIndex = prefixSize + 1;

		const Block &params = interest->getParameters();
		if (0 < params.elements_size()) {
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
		if (iter != m_PAMap.end()) {
			m_PAMap.erase(iter);
		}

		Name dataName(interestName);

		Block empty;
		sendData(dataName, empty);
		m_nRNsnPUDataCount += 1;

		// keyHash 를 저장할 RN-{yyy} 을 찾아서,
		// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.
        //m_rnTopic->receiveInterestPUTopic(keyHash);
		return;
	} else if (cmd.compare("TS") == 0) {
		m_nRNrvTSInterestCount += 1;

		NS_LOG_DEBUG("recv interest: " << interestName);
		//
		int32_t attentionIndex = prefixSize + 1;

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		string topic = ::ndn::unescape(topicName.toUri());

		std::stringstream sio;
		StringMapIterator mapIter;
		int at = topic.find('*');
		if (at < 0) {
			mapIter = m_PAMap.find(topic);
			if (mapIter != m_PAMap.end()) {
				sio << mapIter->second;
			}
		} else {
			string exp(topic);
			exp.insert(at, 1, '.');
			int count = 0;
			std::regex reg(exp);
			for ( mapIter = m_PAMap.begin(); mapIter != m_PAMap.end(); mapIter ++) {
				if (std::regex_match(mapIter->first, reg)) {
					if (0 < count) {
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

		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)nodes.c_str(), nodes.size());
		sendData(dataName, buffer);
		m_nRNsnTSDataCount += 1;

		return;
	} else if (cmd.compare("TM") == 0) {
		m_nRNrvTMInterestCount += 1;

		NS_LOG_DEBUG("recv interest: " << interestName);
		//
		int32_t attentionIndex = prefixSize + 1;
		if(0 < sendDataTM(interestName, attentionIndex)) {
			removePendingTimeoutEvent(interestName);
		}
		return;
		// string value = m_rnDht->receiveInterestSendSuccessorList();
	} else	if (cmd.compare("DM") == 0) {
		m_nRNrvDMInterestCount += 1;

		NS_LOG_DEBUG("recv interest: " << interestName);
		// Data Manifest Request
		int32_t attentionIndex = prefixSize + 1;

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		string topic = topicName.toUri();


		BufferListMapIterator mapIter = m_DPMap.find(topic);
		BufferPtr ptr = nullptr;
		if (mapIter != m_DPMap.end()) {
			BufferListPtr listPtr = mapIter->second;
            //NS_LOG_DEBUG(topic << ": " << listPtr->size());

			std::stringstream sio;
			BufferListIterator iter = listPtr->begin();
			for (; iter != listPtr->end(); iter ++) {
				std::string tmp((const char *)(*iter)->data(), (*iter)->size());
				sio << tmp << ", ";
			}
			NS_LOG_DEBUG(topic << ": " << sio.str());

			// 마지막 1개만 제공한다.
			ptr = listPtr->at(listPtr->size()-1);
		} else {
			// 없으면 나중에 DP interest를 수신하여 pending list에서 찾아 전송한다.
			return;
		}
        /*
		size_t topicSize = topicName.size();
		::ndn::Block content(::ndn::tlv::Content);
		mapIter = m_DPMap.begin();
		for (; mapIter != m_DPMap.end(); mapIter ++) {
			// /a/b/c/topic-0/0
			Name publishTopicName(mapIter->first);
			if (publishTopicName.size() < topicSize) {
				continue;
			}

			if (publishTopicName.getPrefix(prefixSize).equals(topicName) == false) {
				continue;
			}
			if (publishTopicName.equals(topicName) == false) {
				continue;
			}
			::ndn::Block nameBlock(::ndn::tlv::Name, Block((uint8_t *)mapIter->first.c_str(), mapIter->first.size()));
			content.push_back(nameBlock);
		}
        */

		Name tmDataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(tmDataName);
        //data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

        //data->setContent(content);

		if (ptr != nullptr) {
			data->setContent(ptr);
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

		NS_LOG_DEBUG("send data: " << tmDataName);

		// data sent time
		m_appLink->onReceiveData(*data);
		m_nRNsnDMDataCount += 1;

		removePendingTimeoutEvent(data->getName());
	} else	if (cmd.compare("DR") == 0) {
		m_nRNrvDRInterestCount += 1;

		NS_LOG_DEBUG("recv interest: " << interestName);
		// Data Request
		// Data Manifest Request
		int32_t attentionIndex = prefixSize + 1;

		Name qualifiedName = interestName.getSubName(attentionIndex, interestName.size()-1);
		NS_LOG_DEBUG("qualifiedName: " << qualifiedName);
		Name topicName = qualifiedName.getSubName(0, qualifiedName.size()-1);
		string topic = topicName.toUri();
		string seqStr = qualifiedName.get(qualifiedName.size()-1).toUri();

		Name tmDataName(interestName);
		// generate data pacaket
		auto data = make_shared<Data>();
		data->setName(tmDataName);
        //data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		BufferListMapIterator mapIter = m_DPMap.find(topic);
		if (mapIter != m_DPMap.end()) {
			// 원래는 Producer가 publish 하는 내용을 Data의 content로 해야 하나
			// 여기에서는 publish되었는지 확인만 하고 데이터는 생성하여 제공한다.
#if 0
			// /a/b/c/topic-0/0
			::ndn::Block content(::ndn::tlv::Content);

			// std::vector<std::shared_ptr<ndn::Buffer> >::iterator

			std::shared_ptr<std::vector<BufferPtr>> second = mapIter->second;

			BufferListPtr dataListPtr = mapIter->second;
			BufferListIterator iter = dataListPtr->begin();
			for (int i = 0; iter != dataListPtr->end(); iter ++, i++) {
				::ndn::Block data(::ndn::tlv::AppPrivateBlock1+i, *iter);
				content.push_back(data);
			}
			data->setContent(content);
#else
			BufferListPtr bufferList = mapIter->second;

			BufferListIterator bufferIter = bufferList->begin();
			for (; bufferIter != bufferList->end(); bufferIter ++) {
				BufferPtr buffer = *bufferIter;
				string tmpSeq((const char *)buffer->data(), buffer->size());

				if (seqStr.compare(tmpSeq) == 0) {
					shared_ptr<::ndn::Buffer> buffer = make_shared<::ndn::Buffer>(m_nDataSize);
					::memset(buffer->get<uint8_t>(), 'a', buffer->size());
                    //::ndn::Block content(::ndn::tlv::AppPrivateBlock1, buffer);
					data->setContent(buffer);
					break;
				}
			}
#endif
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
		m_nRNsnDRDataCount += 1;

		removePendingTimeoutEvent(data->getName());
	}
}

void
RendezvousDrnF::sendDataForPendingInsterestTM(const Name &interestName) {
	// Rendezvous로부터 PA Interest를 받으면(/RN-xxxxx/PA/a/b/c/topic-0)
	// Pending된 TM interest(/RN-xxxxx/TM/a/b/c/topic-0) 에 대한 Data를 전송한다.
	int32_t prefixSize = m_rnPrefix.size();

	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("PA") != 0) {
		return;
	}

	int32_t attentionIndex = prefixSize + 1;

	// /RN-xxxxx/PA/a/b/c/topic-0 -> /a/b/c/topic-0
	Name paTopicName = interestName.getSubName(attentionIndex, Name::npos);
	string paTopic = paTopicName.toUri();
	NS_LOG_DEBUG(paTopic);

	std::vector<::ndn::Name> removeNameList;
	// pending 목록에서 publish 된 topic을 찾는다.
	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>>::iterator pendingIter;
	pendingIter = m_pendingEvent.begin();
	for ( ; pendingIter != m_pendingEvent.end(); pendingIter ++) {
		int32_t drnPrefixSize = m_drnPrefix.size();
		int32_t drnAttentionIndex = drnPrefixSize + 1;

		string pendingCmd = pendingIter->first.get(drnPrefixSize).toUri();
		if(pendingCmd.compare(string("TM")) != 0) {
			continue;
		}

		// pending Interest name이
		// /RN-xxxxx/TM/a/b/c
		// /RN-xxxxx/TM/a/b/*
		// 등 여러 개 일 수 있으니.
		Name tmTopicName = pendingIter->first.getSubName(drnAttentionIndex, pendingIter->first.size()-1);
		string tmTopic = tmTopicName.toUri();

		int at = tmTopic.find('*');
		if (at < 0) {
			// '*'가 없다면 같은 경우
			if (tmTopic.compare(paTopic) == 0) {
				if(0 < sendDataTM(pendingIter->first, drnAttentionIndex)) {
					removeNameList.push_back(pendingIter->first);
				}
			}
		} else {
			string exp(tmTopic);
			exp.insert(at, 1, '.');
			std::regex reg(exp);

			// PA의 topic이 TM의 topic이름에 match 되는지 확인한다.
			if (std::regex_match(paTopic, reg)) {
				if( 0 < sendDataTM(pendingIter->first, drnAttentionIndex)) {
					removeNameList.push_back(pendingIter->first);
				}
			}
		}
	}

	std::vector<::ndn::Name>::iterator nameIter;
	nameIter = removeNameList.begin();
	for ( ; nameIter != removeNameList.end(); nameIter ++) {
		removePendingTimeoutEvent(*nameIter);
	}
}

void
RendezvousDrnF::sendDataForPendingInsterestDM(const Name &interestName) {
	// Producer로부터 DP Interest를 받으면(/RN/DP/a/b/c/topic-0/0)
	// Pending된 DM interest(/RN-xxxxx/DM/a/b/c/topic-0) 에 대한 Data를 전송한다.
	int32_t prefixSize = m_rnPrefix.size();

	string cmd = interestName.get(prefixSize).toUri();
	if (cmd.compare("DP") != 0) {
		return;
	}

	int32_t attentionIndex = prefixSize + 1;

	// /RN/DP/a/b/c/topic-0/0 -> /a/b/c/topic-0
	Name dpQualifiedName = interestName.getSubName(attentionIndex, interestName.size()-1);
	NS_LOG_DEBUG(dpQualifiedName);

	// /a/b/c/topic-0
	Name dpTopicName = dpQualifiedName.getSubName(0, dpQualifiedName.size()-1);
	string dpTopic = dpTopicName.toUri();
	// sequence: 0
	string seqStr = dpQualifiedName.get(dpQualifiedName.size()-1).toUri();

	// std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>> m_pendingEvent

	std::vector<::ndn::Name> removeNameList;
	// pending 목록에서 publish 된 topic을 찾는다.
	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>>::iterator pendingIter;
	pendingIter = m_pendingEvent.begin();
	for ( ; pendingIter != m_pendingEvent.end(); pendingIter ++) {
		int32_t drnPrefixSize = m_drnPrefix.size();
		int32_t drnAttentionIndex = drnPrefixSize + 1;

		string pendingCmd = pendingIter->first.get(drnPrefixSize).toUri();
		if(pendingCmd.compare(string("DM")) != 0) {
			continue;
		}
		//
		Name dmTopicName = pendingIter->first.getSubName(drnAttentionIndex, pendingIter->first.size()-1);
		string dmTopic = dmTopicName.toUri();

		// DM
		if (dmTopic.compare(dpTopic) != 0) {
			continue;
		}

		//
		BufferListMapIterator mapIter = m_DPMap.find(dpTopic);
		BufferPtr ptr = nullptr;

		if (mapIter != m_DPMap.end()) {
			BufferListPtr listPtr = mapIter->second;
			NS_LOG_DEBUG(dpTopic << ": " << listPtr->size());

			if (listPtr->size() == 0) {
				// 보낼 topic이 없다.
				continue;
			}

			std::stringstream sio;
			BufferListIterator iter = listPtr->begin();
			for (; iter != listPtr->end(); iter ++) {
				std::string tmp((const char *)(*iter)->data(), (*iter)->size());
				sio << tmp << ", ";
			}
			NS_LOG_DEBUG(dpTopic << ": " << sio.str());

			// 마지막 1개만 제공한다.
			ptr = listPtr->at(listPtr->size()-1);

			Name tmDataName(pendingIter->first);
			// generate data pacaket
			auto data = make_shared<Data>();
			data->setName(tmDataName);

			if (ptr != nullptr) {
				data->setContent(ptr);
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

			NS_LOG_DEBUG("send data: " << tmDataName);
			// data sent time
			m_appLink->onReceiveData(*data);
			removeNameList.push_back(data->getName());
		} else {
			continue;
		}
	}

	std::vector<::ndn::Name>::iterator nameIter;
	nameIter = removeNameList.begin();
	for ( ; nameIter != removeNameList.end(); nameIter ++) {
		removePendingTimeoutEvent(*nameIter);
	}
}

void
RendezvousDrnF::OnInterest(shared_ptr<const Interest> interest) {
	const Name &interestName = interest->getName();

	// /RN-xxxxx/TM/a/b/c <---- /RN-xxxxx/PA/a/b/c
	appendPendingTimeoutEvent(interest);

	const name::Component &topPrefix = interestName.get(0);
	string prefix = topPrefix.toUri();

	NS_LOG_DEBUG("recv interest: " << interestName);

	if (prefix == m_rnPrefix) {
		OnInterestRN(interest);
	} else if (prefix == m_drnPrefix) {
		OnInterestRNXXX(interest);
	}
}

void
RendezvousDrnF::OnData(shared_ptr<const Data> data) {
	const Name &dataName = data->getName();

    //NS_LOG_DEBUG("recv data: " << dataName);

	// /RN/PA/..., /RN/PU/..., /RN/DP/..., /RN/TS/..., /RN/TM/...,
	// /RN-{yyyyy}/PA/..., /RN-{yyyyy}/PU/..., /RN-{yyyyy}/DP/..., /RN-{yyyyy}/TS/..., /RN-{yyyyy}/TM/...,
	int32_t prefixSize = 1;
	int32_t cmdIndex = prefixSize;

	string cmd = dataName.get(cmdIndex).toUri();
	if (cmd.compare("TS") == 0) {
		m_nRNrvTSDataCount += 1;
		NS_LOG_DEBUG("recv data: " << dataName);
		// /RN-{yyyyy}/TS/...에  대한 응답(Data)
		int32_t attentionIndex = prefixSize + 1;
		Name topicName = dataName.getSubName(attentionIndex, Name::npos);
	} else if (cmd.compare("PA") == 0) {
		m_nRNrvPADataCount += 1;
		NS_LOG_DEBUG("recv data: " << dataName);
		// /RN-{yyyyy}/PA/...에  대한 응답(Data)
		int32_t attentionIndex = prefixSize + 1;
		Name topicName = dataName.getSubName(attentionIndex, Name::npos);
	} else if (cmd.compare("PU") == 0) {
		m_nRNrvPUDataCount += 1;
		NS_LOG_DEBUG("recv data: " << dataName);
		// /RN-{yyyyy}/PU/...에  대한 응답(Data)
		int32_t attentionIndex = prefixSize + 1;
		Name topicName = dataName.getSubName(attentionIndex, Name::npos);
	} else if (cmd.compare("TM") == 0) {
		m_nRNrvTMDataCount += 1;
		NS_LOG_DEBUG("recv data: " << dataName);
		// /RN-{yyyyy}/TM/...에  대한 응답(Data: /RN/TM/...)
		int32_t attentionIndex = prefixSize + 1;
		Name topicName = dataName.getSubName(attentionIndex, Name::npos);

		Name tmpName("RN");
		tmpName.append(cmd).append(topicName);

		const Block &content = data->getContent();
		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const uint8_t *)content.value(), content.value_size());

		sendData(tmpName, buffer);
		m_nRN__TMDataCount += 1;
	}
}

void
RendezvousDrnF::OnNack(shared_ptr<const ::ndn::lp::Nack> nack) {
	const Interest &interest = nack->getInterest();
	const Name &interestName = interest.getName();

	int32_t rnPrefixSize = m_rnPrefix.size();
	int32_t drnPrefixSize = m_drnPrefix.size();

	if (m_rnPrefix.equals(interestName.getSubName(0, rnPrefixSize))) {
		int32_t cmdIndex = rnPrefixSize;
		string cmd = interestName.get(cmdIndex).toUri();

		if (cmd.compare("TS") == 0) {
			m_nRNsnTSNackCount += 1;
		} else	if (cmd.compare("TM") == 0) {
			m_nRNsnTMNackCount += 1;
		}
	} else if (m_drnPrefix.equals(interestName.getSubName(0, drnPrefixSize))) {
		int32_t cmdIndex = drnPrefixSize;
		string cmd = interestName.get(cmdIndex).toUri();

		if (cmd.compare("PA") == 0) {
			m_nRNsnPANackCount += 1;
		} else	if (cmd.compare("PU") == 0) {
			m_nRNsnPUNackCount += 1;
		}
	}
}

bool
RendezvousDrnF::IsPendingTarget(const Name &name) {
	int32_t prefixSize = 1;
	int32_t cmdIndex = prefixSize;

	if(name.size() < 2) {
		return false;
	}

	const name::Component &topPrefix = name.get(0);
	string prefix = topPrefix.toUri();

	if(m_drnPrefix.equals(Name(prefix))) {
		string cmd = name.get(cmdIndex).toUri();
		if (cmd.compare("TM") == 0) {
			return true;
		} if (cmd.compare("DM") == 0) {
			return true;
		}
	}

	return TimeoutApp::IsPendingTarget(name);
}
