/*
 * consumer.cpp
 *
 *  Created on: 2019. 9. 20.
 *      Author: root
 */

#include "rendezvous-dht.hpp"

#include <ns3/ndnSIM/ndn-cxx/encoding/block.hpp>
#include <ns3/ndnSIM/ndn-cxx/interest.hpp>
#include <ns3/ndnSIM/ndn-cxx/name.hpp>
#include <ns3/event-id.h>
#include <ns3/object.h>
#include <ns3/nstime.h>
#include <ns3/simulator.h>
#include <stddef.h>

#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

#include "utils.hpp"
#include "../Chord-DHT/headers.h"
#include "../Chord-DHT/helperClass.h"

NS_LOG_COMPONENT_DEFINE("drn.RendezvousDHT");

//using namespace ns3::ndn;
NS_OBJECT_ENSURE_REGISTERED(RendezvousDHT);

ns3::TypeId
RendezvousDHT::GetTypeId() {
  static ns3::TypeId tid = ns3::TypeId("RendezvousDHT")
						.SetGroupName("Ndn")
						.SetParent<ns3::Object>()
							;

  return tid;
}

RendezvousDHT::RendezvousDHT(const ::ndn::Name& drnPrefix, const ::ndn::Name& predecessor, ns3::ndn::AppLinkService *appLink, int maxNode)
	: m_drnPrefix(drnPrefix)
	, m_predecessor(predecessor)
	, m_appLink(appLink)
	, m_signature(0U)
	, m_dhtNode(drnPrefix.get(0).toUri(), maxNode)
{
}

RendezvousDHT::~RendezvousDHT() {

}

void
RendezvousDHT::sendData(shared_ptr<const Interest> interest, const Block &content) {
	Name dataName(interest->getName());
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	if(0 < content.value_size() || 0 < content.elements_size()) {
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

void
RendezvousDHT::sendData(shared_ptr<const Interest> interest, shared_ptr<const ::ndn::Buffer> &value) {
	Name dataName(interest->getName());
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	if(0 < value->size()) {
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
RendezvousDHT::create() {
	std::string drnPrefix = m_drnPrefix.toUri();
	std::string predecessor = m_predecessor.toUri();

	/* key to be hashed for a node is RN-{nnn} */
	lli nodeId = Helper::getHash(m_dhtNode.m_nodeName);

	/* setting id, successor , successor list , predecessor ,finger table and status of node */
	m_dhtNode.setId(nodeId);
	m_dhtNode.setSuccessor(m_dhtNode.m_nodeName, nodeId);
	m_dhtNode.setSuccessorList(m_dhtNode.m_nodeName, nodeId);
	m_dhtNode.setPredecessor("", -1);
	m_dhtNode.setFingerTable(m_dhtNode.m_nodeName, nodeId);
	m_dhtNode.setStatus();

	int step = 0;
#if 1
	ns3::Time delay(ns3::MilliSeconds(0));
	ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
	doStabilize(step);
#endif
}

void
RendezvousDHT::sendInterestJoin() {
	/* generate id of current node */
	string name = m_dhtNode.m_nodeName;

	// RN-{yyy}/join/RN-{xxx}
	::ndn::Name interestName(m_predecessor);
	interestName.append("join").append("successor").append(name);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
}

void
RendezvousDHT::receiveInterestJoinSuccessor(shared_ptr<const Interest> interest, int32_t subcmdIndex) {
	const Name &interestName = interest->getName();
	int32_t nodeNameIndex = subcmdIndex + 1;
	string nodeName = interestName.get(nodeNameIndex).toUri();
	lli nodeId = Helper::getHash(nodeName);
	pair<string, lli> successor;

	// tuple - 0th: result, 1th: index
	// 0th - -1 : can not find and stop, 1th: index
	// 0th - 0 : find and stop, 1th: index
	// 0th - 1 : can not find, 1th: next index
	std::tuple<int, int> result;
	result = m_dhtNode.findSuccessor(nodeId, successor);
	if(std::get<0>(result) < 0) {
		result = m_dhtNode.closestPrecedingNode(nodeId, M, successor);
		// 찾은 것이 successor가 아니라 내가.... 찾아졌다면 나의 successor를 보내야 함.
		if(std::get<0>(result) == 0) {
			if(successor.second == m_dhtNode.getId()){
				successor = m_dhtNode.getSuccessor();
			}
		} else if(std::get<0>(result) < 0) {
			/*
			 * if this node couldn't find closest preciding node
			 * for given node id then now ask it's successor to do so
			 */
			successor = m_dhtNode.getSuccessor();
		}
	}

	if(std::get<0>(result) == 0) {
		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		shared_ptr<const ::ndn::Buffer> buffer;
		buffer = make_shared<::ndn::Buffer>((const void*)successor.first.c_str(), successor.first.size());
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

		NS_LOG_DEBUG("send data: " << dataName);

		// data sent time
		m_appLink->onReceiveData(*data);

		return;
	} else if(0 < std::get<0>(result)) {
		// 못 찾으면
		// RN-???/join/finger/{n}/{node name}
		::ndn::Name name(successor.first);
		name.append("join").append("finger");
		name.append(stringf("%d", std::get<1>(result)));
		name.append(nodeName);
		// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);

		interest->setName(name);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		// send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	}

	return;
}

void
RendezvousDHT::receiveInterestJoinFinger(shared_ptr<const Interest> interest, int32_t subcmdIndex) {
	const Name &interestName = interest->getName();
//	int32_t attentionIndex = subcmdIndex + 1;
//	string nodeName = interestName.get(attentionIndex).toUri();

	pair<string, lli > successor = m_dhtNode.getSuccessor();

	Name dataName(interestName);
	// generate data pacaket
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	//
	shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)successor.first.c_str(), successor.first.size());
	data->setContent(buffer);

#if  1
	Signature signature;
	SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

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
RendezvousDHT::receiveInterestJoinGetKeys(shared_ptr<const Interest> interest, int32_t subcmdIndex) {
	const Name &interestName = interest->getName();
	int32_t nodeNameIndex = subcmdIndex + 1;
	string nodeName = interestName.get(nodeNameIndex).toUri();
	lli idHash = Helper::getHash(nodeName);
	vector< pair<lli , string> > keysAndValuesVector = m_dhtNode.getKeysForPredecessor(idHash);

	std::stringstream sio;
    /* will arrange all keys and val in form of key1:val1;key2:val2; */
    for(size_t i=0;i<keysAndValuesVector.size();i++){
		if(0 < i) {
			sio << ';';
		}
    	sio << to_string(keysAndValuesVector[i].first) + ":" + keysAndValuesVector[i].second;
    }

    string keysAndValues = sio.str();

	Name dataName(interestName);
	// generate data pacaket
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	//
	shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)keysAndValues.c_str(), keysAndValues.size());
	data->setContent(buffer);

#if  1
	Signature signature;
	SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

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
RendezvousDHT::receiveDataJoinSuccessor(shared_ptr<const Data> data, int32_t subcmdIndex) {
	const Block &content = data->getContent();
	string successor = string((const char *)content.value(), content.value_size());

	NS_LOG_DEBUG("contents: successor: " << successor);

	lli hash = Helper::getHash(successor);

	lli nodeId = Helper::getHash(m_dhtNode.m_nodeName);

	m_dhtNode.setId(nodeId);
	m_dhtNode.setSuccessor(successor, hash);
	m_dhtNode.setSuccessorList(successor, hash);
	m_dhtNode.setPredecessor("",-1);
	m_dhtNode.setFingerTable(successor, hash);
	m_dhtNode.setStatus();

	// successor에 key 목록을 묻는다.
	sendInterestJoinGetKeys(successor);
}

void RendezvousDHT::sendInterestLeave() {
	pair<string, lli> successor = m_dhtNode.getSuccessor();

    lli id = m_dhtNode.getId();
    if(id == successor.second) {
        return;
    }

    vector< pair<lli , string> > keysAndValuesVector = m_dhtNode.getAllKeysForSuccessor();
    if(keysAndValuesVector.size() == 0) {
        return;
    }

    string keysAndValues = "";
    /* will arrange all keys and val in form of key1:val1;key2:val2; */
    for(size_t i=0;i<keysAndValuesVector.size();i++){
        keysAndValues += to_string(keysAndValuesVector[i].first) + ":" + keysAndValuesVector[i].second;
        keysAndValues += ";";
    }

//    keysAndValues += "storeKeys";

    //RN-nnn/sendSuccList
	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);
	interest->setName("storeKeys");
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//    interest->setInterestLifetime(interestLifeTime);

    interest->setParameters((const uint8_t *)keysAndValues.c_str(), keysAndValues.size());

    m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveDataLeave(string successor) {

}

void RendezvousDHT::receiveInterestStoreAllKeys(string keyAndValues) {
	Helper::storeAllKeys(m_dhtNode, keyAndValues);
}

void RendezvousDHT::doStabilize(int step) {
	// 각 단계에서 여러 번 데이터를 주고 받으므로 작업을 유지하기 위하여 Name에 목적을 표시한다.

	pair< string, lli > xcessor;
	std::tuple<int, int> result;

//	m_dhtNode.checkPredecessor();
	if(step == 0) {
		// Predecessor를 점검
		result = m_dhtNode.checkPredecessor(xcessor);
		if (0 < std::get<0>(result)) {
			// Predecessor가 설정 되어 있으면, Predecessor가 동작 중인지 확인한다.
			ns3::Time delay(ns3::MilliSeconds(0));
			m_stablilizeEventId = ns3::Simulator::Schedule(delay, &RendezvousDHT::sendInterestPredecessorAlive, this, xcessor.first);
			return;
		}
		step = 1;
	}

//	m_dhtNode.checkSuccessor();
	if(step == 1) {
		result = m_dhtNode.checkSuccessor(xcessor);
		if (0 < std::get<0>(result)) {
			ns3::Time delay(ns3::MilliSeconds(0));
			m_stablilizeEventId = ns3::Simulator::Schedule(delay, &RendezvousDHT::sendInterestSuccessorAlive, this, xcessor.first);
			return;
		}
		step = 2;
	}

//	m_dhtNode.stabilize();
	if(step == 2) {
		xcessor = m_dhtNode.getSuccessor();
		sendInterestStabilizeAlive(xcessor.first);
		return;
//		step = 3;
	}

//	m_dhtNode.updateSuccessorList(list);
	if(step == 3) {
		xcessor = m_dhtNode.getSuccessor();
		sendInterestUpdateSuccessorList();
		return;
//		step = 4;
	}

//	m_dhtNode.fixFingers();
	if(step == 4) {
		xcessor = m_dhtNode.getSuccessor();
		sendInterestFixFingersAlive(xcessor.first, 1);
		return;
//		step = 5;
	}

	if(step == 5) {
		ns3::Time delay(ns3::MilliSeconds(300));
		ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, 0);
	}
}

void RendezvousDHT::sendInterestPredecessorAlive(string nodeName) {
	if (m_drnPrefix.equals(Name(nodeName))) {
		int step = 1;
#if 1
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
		doStabilize(step);
#endif
		return;
	}
	::ndn::Name interestName(nodeName);
	interestName.append("stabilize").append("checkPredecessor").append("alive");

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveDataPredecessorAlive(shared_ptr<const Data> data, int32_t subcmdIndex) {
	// 응답이 왔다는 것은 살아 있다는 것.
	const Block &content = data->getContent();
	string alive((const char *)content.value(), content.value_size());

	NS_LOG_DEBUG(data->getName() << " : " << alive);

	int step = 1;
#if 1
	ns3::Time delay(ns3::MilliSeconds(0));
	ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
	doStabilize(step);
#endif
}

void RendezvousDHT::setDeadPredecessor() {
	m_dhtNode.setDeadPredecessor();
}

void RendezvousDHT::sendInterestCheckSuccessorUpdateSuccessorList() {
	m_dhtNode.updateSuccessor();

	pair<string, lli > successor = m_dhtNode.getSuccessor();
	Name interestName(successor.first);
	interestName.append("stabilize");
	interestName.append("checkSuccessor");
	interestName.append("sendSuccList");

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::sendInterestSuccessorAlive(string nodeName) {
	if (m_drnPrefix.equals(Name(nodeName))) {
		int step = 2;
#if 1
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
		doStabilize(step);
#endif
		return;
	}
	::ndn::Name interestName(nodeName);
	interestName.append("stabilize");
	interestName.append("checkSuccessor");
	interestName.append("alive");

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveDataSuccessorAlive(shared_ptr<const Data> data, int32_t subcmdIndex) {
	// 응답이 왔다는 것은 살아 있다는 것.
	const Block &content = data->getContent();
	string alive((const char *)content.value(), content.value_size());

	NS_LOG_DEBUG(data->getName() << " : " << alive);

	int step = 2;
#if 1
	ns3::Time delay(ns3::MilliSeconds(0));
	ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
	doStabilize(step);
#endif
}

void RendezvousDHT::receiveInterestSendSuccessorList(shared_ptr<const Interest> interest, int32_t cmdIndex) {
	const Name &interestName = interest->getName();
	string cmd = interestName.get(cmdIndex).toUri();
	if(cmd.compare("sendSuccList") == 0) {
		vector< pair<string, lli > > list = m_dhtNode.getSuccessorList();
		string successorList = Helper::splitSuccessorList(list);

		Name dataName(interestName);
		auto data = make_shared<Data>();
		data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

		shared_ptr<const ::ndn::Buffer> buffer = make_shared<::ndn::Buffer>((const void*)successorList.c_str(), successorList.size());
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
	}
}

void RendezvousDHT::receiveDataSendSuccessorList(shared_ptr<const Data> data, int32_t cmd3thIndex) {
	const Block &content = data->getContent();

	string successorList((const char *)content.value(), content.value_size());

	vector<string> list = Helper::seperateSuccessorList(successorList);
	m_dhtNode.updateSuccessorList(list);
}

void RendezvousDHT::sendInterestStabilizeAlive(string nodeName) {
	//
	if(m_drnPrefix.equals(Name(nodeName))) {
		pair<string, lli > successor = m_dhtNode.getSuccessor();
		sendInterestStabilizeP1(successor.first);
		return;
	}

	::ndn::Name interestName(nodeName);
	interestName.append("stabilize");
	interestName.append("stabilize");
	interestName.append("alive");

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
	return;
}

void RendezvousDHT::receiveDataStabilizeAlive(shared_ptr<const Data> data, int32_t cmd3thIndex) {
	pair<string, lli > successor = m_dhtNode.getSuccessor();
	sendInterestStabilizeP1(successor.first);
}

void RendezvousDHT::sendInterestStabilizeP1(string nodeName) {
	if(m_drnPrefix.equals(Name(nodeName))) {
		pair<string, lli > self(nodeName, m_dhtNode.getId());
		m_dhtNode.notify(self);

		pair<string, lli > predecessor = m_dhtNode.getPredecessor();
		lli predecessorHash = predecessor.second;
		string predecessorName = predecessor.first;

		pair<string, lli > myPredecessor = m_dhtNode.getPredecessor();
		pair<string, lli > mySuccessor = m_dhtNode.getSuccessor();

		if(predecessorHash == -1 || myPredecessor.second == -1) {
			int step = 3;
	#if 1
			ns3::Time delay(ns3::MilliSeconds(0));
			ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
	#else
			doStabilize(step);
	#endif
			return;
		}

		lli myId = m_dhtNode.getId();
		if(predecessorHash > myId || (predecessorHash > myId && predecessorHash < mySuccessor.second) || (predecessorHash < myId && predecessorHash < mySuccessor.second)){
			m_dhtNode.setSuccessor(predecessorName, predecessorHash);
		}

		int step = 3;
	#if 1
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
	#else
		doStabilize(step);
	#endif

		return;
	}
	// RN-yyyyy/stabilize/stabilize/p1/RN-xxxxx
	::ndn::Name interestName(nodeName);
	interestName.append("stabilize");
	interestName.append("stabilize");
	interestName.append("p1");
	interestName.append(m_drnPrefix);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveInterestStabilizeP1(shared_ptr<const Interest> interest, int32_t cmdIndex) {
	const Name &interestName = interest->getName();
	int32_t nodeIndex = cmdIndex + 1;
	string nodeName = interestName.get(nodeIndex).toUri();

	pair<string, lli > predecessor = m_dhtNode.getPredecessor();;

	string key =predecessor.first;
	string value = std::to_string(predecessor.second);

	shared_ptr<const ::ndn::Buffer> buffer;

	buffer = make_shared<::ndn::Buffer>((const void*)key.c_str(), key.size());
	ndn::Block keyBlock(128, buffer); // key

	buffer = make_shared<::ndn::Buffer>((const void*)value.c_str(), value.size());
	ndn::Block valueBlock(129, buffer); // value

	ndn::Block content(ndn::tlv::Content);
	content.push_back(keyBlock);
	content.push_back(valueBlock);
	content.encode();

//	shared_ptr<const ::ndn::Buffer> buffer;
//	buffer = make_shared<::ndn::Buffer>((const void*)predecessor.first.c_str(), predecessor.first.size());
	sendData(interest, content);

	lli nodeId = Helper::getHash(nodeName);
	pair<string, int> node;
	node.first = nodeName;
	node.second = nodeId;

	m_dhtNode.notify(node);
}

void RendezvousDHT::receiveDataStabilizeP1(shared_ptr<const Data> data, int32_t cmd3ndIndex) {
	const Block &content = data->getContent();
	content.parse();

	const Block &keyBLock = content.get(128);
	const Block &valueBlock = content.get(129);
	lli predecessorHash = std::stoll(string((const char *)valueBlock.value(), valueBlock.value_size()));
	string predecessor((const char *)keyBLock.value(), keyBLock.value_size());

	pair<string, lli > myPredecessor = m_dhtNode.getPredecessor();
	pair<string, lli > mySuccessor = m_dhtNode.getSuccessor();

	if(predecessorHash == -1 || myPredecessor.second == -1) {
		int step = 3;
#if 1
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
		doStabilize(step);
#endif
		return;
	}

	lli myId = m_dhtNode.getId();
	if(predecessorHash > myId || (predecessorHash > myId && predecessorHash < mySuccessor.second) || (predecessorHash < myId && predecessorHash < mySuccessor.second)){
		m_dhtNode.setSuccessor(predecessor, predecessorHash);
	}

	int step = 3;
#if 1
	ns3::Time delay(ns3::MilliSeconds(0));
	ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
	doStabilize(step);
#endif
}

void RendezvousDHT::sendInterestUpdateSuccessorList() {
	pair<string, lli > successor = m_dhtNode.getSuccessor();
	if(m_drnPrefix.equals(Name(successor.first))) {
		int step = 4;
	#if 1
		ns3::Time delay(ns3::MilliSeconds(0));
		ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
	#else
		doStabilize(step);
	#endif
		return;
	}

	Name interestName(successor.first);
	interestName.append("stabilize");
	interestName.append("updateSuccessorList");
	interestName.append("sendSuccList");

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest > ();
	interest->setName(interestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << interestName);

	m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveInterestUpdateSuccessorList(shared_ptr<const Data> data, int32_t cmdIndex) {
	const Block &content = data->getContent();

	string successorList((const char *)content.value(), content.value_size());

	vector<string> list = Helper::seperateSuccessorList(successorList);
	m_dhtNode.updateSuccessorList(list);

	int step = 4;
#if 1
	ns3::Time delay(ns3::MilliSeconds(0));
	ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
	doStabilize(step);
#endif
	return;
}

void RendezvousDHT::sendInterestFixFingersAlive(string nodeName, int fingerIndex) {
	if(m_drnPrefix.equals(Name(nodeName))) {

		lli mod = pow(2,M);
		lli newId = m_dhtNode.getId() + pow(2,fingerIndex-1);
		newId = newId % mod;

		pair<string, lli > successor;
		tuple<int, int> result = m_dhtNode.findSuccessor(newId, successor);
		if(std::get<0>(result) < 0) {
			result = m_dhtNode.closestPrecedingNode(newId, M, successor);
		}
		if(std::get<0>(result) == 0) {
			if(successor.first == "" || successor.second == -1) {
				int step = 5;
	#if 1
				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
	#else
				doStabilize(step);
	#endif
				return;
			}

			m_dhtNode.setFingerTable(fingerIndex, successor.first, successor.second);

			if(fingerIndex < M) {
				pair< string, lli > xcessor;
				xcessor = m_dhtNode.getSuccessor();
				sendInterestFixFingersAlive(xcessor.first, fingerIndex+1);
				return;
			} else {
				int step = 5;
	#if 1
				ns3::Time delay(ns3::MilliSeconds(0));
				ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
	#else
				doStabilize(step);
	#endif
				return;
			}
		} else if(0 < std::get<0>(result)) {






		} else {
			// 없다.
		}
	}

	ndn::Name name(nodeName);
	name.append("stabilize");
	name.append("fixFingers");
	name.append("alive");
	name.append(std::to_string(fingerIndex));

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(name);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
    // time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    // interest->setInterestLifetime(interestLifeTime);

    m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveDataFixFingersAlive(shared_ptr<const Data> data, int32_t cmdIndex) {
	const Name &dataName = data->getName();
	int32_t indexIndex = cmdIndex + 1;

	int32_t fingerIndex = std::stoi(dataName.get(indexIndex).toUri());

	lli mod = pow(2,M);
	lli newId = m_dhtNode.getId() + pow(2,fingerIndex-1);
	newId = newId % mod;

	pair<string, lli > successor;
	tuple<int, int> result = m_dhtNode.findSuccessor(newId, successor);
	if(std::get<0>(result) < 0) {
		result = m_dhtNode.closestPrecedingNode(newId, M, successor);
	}
	if(std::get<0>(result) == 0) {
		if(successor.first == "" || successor.second == -1) {
			int step = 5;
#if 1
			ns3::Time delay(ns3::MilliSeconds(0));
			ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
			doStabilize(step);
#endif
			return;
		}

		m_dhtNode.setFingerTable(fingerIndex, successor.first, successor.second);

		if(fingerIndex < M) {
			pair< string, lli > xcessor;
			xcessor = m_dhtNode.getSuccessor();
			sendInterestFixFingersAlive(xcessor.first, fingerIndex+1);
			return;
		} else {
			int step = 5;
#if 1
			ns3::Time delay(ns3::MilliSeconds(0));
			ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, step);
#else
			doStabilize(step);
#endif
			return;
		}
	} else if(0 < std::get<0>(result)) {






	} else {
		// 없다.
	}
}

void RendezvousDHT::sendInterestAlive(string nodeName, Name subCmd) {
	// RN-{nnn}/alive
	ndn::Name name(nodeName);
	name.append("alive");
	name.append(subCmd);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(name);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
    // time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    // interest->setInterestLifetime(interestLifeTime);

    m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveDataAlive(string nodeName, string aliave) {

}

void RendezvousDHT::sendInterestJoinGetKeys(string nodeName) {
	// RN-{nnn}/getKeys/{keyhash}
	Name name(nodeName);
	name.append("join");
	name.append("getKeys");
//	name.append(to_string(m_dhtNode.getId()));
	name.append(m_dhtNode.m_nodeName);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(name);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
	// time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	// interest->setInterestLifetime(interestLifeTime);

	m_appLink->onReceiveInterest(*interest);
}

/**
 * key에 대한 value를 요청하는 Interest를 보낸다.
 */
void RendezvousDHT::sendInterestGet(string key) {
	lli keyHash = Helper::getHash(key);

	// keyHash에 대하여 노드의 ID에 대하여 현재 노드의 successor, predecessor 또는
	// closestPrecedingNode에서 찾는다.(finger테이블 목록을 순회하여 successor, precessor 를 찾는다.
	pair<string, lli > node;
	tuple<int, int> result = m_dhtNode.findSuccessor(keyHash, node);
	if(std::get<0>(result) == 0) {
		// 해당 노드로 부터 key(Hash)에 해당하는 value를 받는다.

		// RN-{nnn}/k/{keyhash}
		ndn::Name name("k");
		name.append(to_string(keyHash));

		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(node.first);
		interest->setName(name);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
	//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	//	interest->setInterestLifetime(interestLifeTime);

	//	send k interest
		m_appLink->onReceiveInterest(*interest);
	}

	// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
}

string RendezvousDHT::receiveInterestGet(string key) {
	lli keyHash = std::stoll(key);
	string value = m_dhtNode.getValue(keyHash);

	return value;
}

void RendezvousDHT::receiveDataGet(string key, string value) {
//	lli keyHash = std::stoll(key);
}

void RendezvousDHT::sendInterestPut(string key, string value) {
	lli keyHash = Helper::getHash(key);

	pair<string, lli > node;
	tuple<int, int> result = m_dhtNode.findSuccessor(keyHash, node);
	if(std::get<0>(result) == 0) {
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(node.first);
		interest->setName("put");
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
	//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	//	interest->setInterestLifetime(interestLifeTime);
	//
	//	ndn::tlv::AppPrivateBlock1 = 128;
	//	ndn::tlv::AppPrivateBlock2 = 32767;
	//	ndn::tlv::ContentType_Blob = 0;

		shared_ptr<const ::ndn::Buffer> buffer;

		// /a/b/c : RN-{nnn}
		buffer = make_shared<::ndn::Buffer>((const void*)key.c_str(), key.size());
		ndn::Block keyBlock(128, buffer); // key

		buffer = make_shared<::ndn::Buffer>((const void*)value.c_str(), value.size());
		ndn::Block valueBlock(129, buffer); // value

		ndn::Block params(ndn::tlv::Parameters);
		params.push_back(keyBlock);
		params.push_back(valueBlock);

		params.encode();

		interest->setParameters(params);

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
	}

	// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
}

void RendezvousDHT::receiveInterestPut(lli keyHash, string value) {
	m_dhtNode.storeKey(keyHash, value);
}

void RendezvousDHT::receiveDataPut(string key) {

}

void RendezvousDHT::receiveDataJoinFinger(shared_ptr<const Data> data, int32_t subcmdIndex) {

	int32_t attentionIndex = subcmdIndex + 1;
	// /RN-{nnn}/join/finger/{index}/{node name}
	const Name &dataName = data->getName();
	Block content = data->getContent();
	string succName((const char *)content.value(), content.value_size());
	lli successorId = Helper::getHash(succName);

	int32_t index = std::stoi(dataName.get(attentionIndex).toUri());

	int32_t nodeNameIndex = attentionIndex + 1;
	string nodeName = dataName.get(nodeNameIndex).toUri();
	lli nodeId = Helper::getHash(nodeName);

	pair<string, lli> successor;
	std::tuple<int, int> result;
	result = m_dhtNode.checkSuccessorNode(nodeId, index, successorId, successor);

	if(std::get<0>(result) == 0) {
		if(successor.second == m_dhtNode.getId()) {

		}
	}

	name::Component topicName = dataName.get(4);
	string topic = topicName.toUri();
	string key((const char *) topic.c_str(), topic.size());
	lli keyHash = std::stoll(key);

	// keyHash 를 저장할 RN-{yyy} 을 찾아서,
	// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.

	if(std::get<0>(result) < 0) {
		// 다시 찾아야 한다.
		// succesor 를 찾을 땐 index = index - 1;
		int next = index-1;
		result = m_dhtNode.closestPrecedingNode(keyHash, next, successor);
	} else if(std::get<0>(result) == 0) {
		// 찾았다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);
		interest->setName("PA");
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		shared_ptr<const ::ndn::Buffer> buffer;

		// /a/b/c : RN-{nnn}
		buffer = make_shared<::ndn::Buffer>((const void*)topic.c_str(), topic.size());
		ndn::Block keyBlock(128, buffer); // key

		string drnPrefix = m_drnPrefix.get(0).toUri();
		buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
		ndn::Block valueBlock(129, buffer); // value

		ndn::Block params(ndn::tlv::Parameters);
		params.push_back(keyBlock);
		params.push_back(valueBlock);

		params.encode();

		interest->setParameters(params);

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else  if(0 < std::get<0>(result)) {
		// predecessor 를 찾아라
		::ndn::Name name(successor.first);
		name.append("finger").append("finger");
		name.append(stringf("%d", std::get<1>(result)));
		name.append(topic);
		// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);

		interest->setName("finger");
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		// send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	}
}

void RendezvousDHT::receiveDataJoinGetKeys(shared_ptr<const Data> data, int32_t subcmdIndex) {
	const Block &content = data->getContent();
	string keysAndValues((const char *)content.value(), content.value_size());

	vector< pair<lli,string> > keysAndValuesVector = Helper::seperateKeysAndValues(keysAndValues);

    for(uint32_t i=0;i<keysAndValuesVector.size();i++){
    	m_dhtNode.storeKey(keysAndValuesVector[i].first , keysAndValuesVector[i].second);
    }

	ns3::Time delay(ns3::MilliSeconds(0));
	ns3::Simulator::Schedule(delay, &RendezvousDHT::doStabilize, this, 0);
}

void RendezvousDHT::receiveInterestP2(shared_ptr<const Interest> interest) {
	Name name = interest->getName();
	pair<string, lli > predecessor = m_dhtNode.getPredecessor();

	Name dataName(interest->getName());
	// generate data pacaket
	auto data = make_shared<Data>();
	data->setName(dataName);
//		data->setFreshnessPeriod(::ndn::time::milliseconds(4000));

	string prodIdStr = to_string(predecessor.second);

	ndn::Block nameBlock(128, Block((uint8_t *)predecessor.first.c_str(), predecessor.first.size())); // key
	ndn::Block hashBlock(129, Block((uint8_t *)prodIdStr.c_str(), prodIdStr.size())); // value
	ndn::Block params(ndn::tlv::Content);
	params.push_back(nameBlock);
	params.push_back(hashBlock);

	data->setContent(params);
#if  0
	Signature signature;
	SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

	if (m_keyLocator.size() > 0) {
	signatureInfo.setKeyLocator(m_keyLocator);
	}

	signature.setInfo(signatureInfo);
	signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

	data->setSignature(signature);
#endif

	// to create real wire encoding
	data->wireEncode();

	// data sent time
	m_appLink->onReceiveData(*data);
}

void RendezvousDHT::receiveDataP2(shared_ptr<const Data> data) {

	// /RN-{nnn}/finger/{index}/PA/topic-{nnn}/.../...
	Name dataName = data->getName();
	Block content = data->getContent();
	ndn::Block nameBlock = content.get(128);
	ndn::Block hashBlock = content.get(129);

	pair<string, lli> predecessor;
	predecessor.first = string((const char *)nameBlock.value(), nameBlock.value_size());
	predecessor.second = atoll((const char *)hashBlock.value_size());

	string cmd = dataName.get(2).toUri();
	string indexStr = dataName.get(3).toUri();
	int  index =atoi(indexStr.c_str());

	name::Component topicName = dataName.get(4);
	string topic = topicName.toUri();
	string key((const char *) topic.c_str(), topic.size());
	lli keyHash = std::stoll(key);

	// keyHash 를 저장할 RN-{yyy} 을 찾아서,
	// /RN-{yyy}/PA/topic-{nnn}[RN-Xxx] Interest를 보낸다.

	pair<string, lli> successor;
	tuple<int, int> result;
	result = m_dhtNode.checkPredecessorNode(keyHash, index, predecessor, successor);

	if(std::get<0>(result) == 0) {
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);
		interest->setName("PA");
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		shared_ptr<const ::ndn::Buffer> buffer;

		// /a/b/c : RN-{nnn}
		buffer = make_shared<::ndn::Buffer>((const void*)topic.c_str(), topic.size());
		ndn::Block keyBlock(128, buffer); // key

		string drnPrefix = m_drnPrefix.get(0).toUri();
		buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
		ndn::Block valueBlock(129, buffer); // value

		ndn::Block params(ndn::tlv::Parameters);
		params.push_back(keyBlock);
		params.push_back(valueBlock);

		params.encode();

		interest->setParameters(params);

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else if( std::get<0>(result) < 0 ) {
		int next = index - 1;
		result = m_dhtNode.closestPrecedingNode(keyHash, next, successor);

		if(std::get<0>(result) == 0) {
			// 찾으면
			std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);
			interest->setName("PA");
			interest->setCanBePrefix(true);
			interest->setMustBeFresh(true);
	//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	//		interest->setInterestLifetime(interestLifeTime);

			shared_ptr<const ::ndn::Buffer> buffer;

			// /a/b/c : RN-{nnn}
			buffer = make_shared<::ndn::Buffer>((const void*)topic.c_str(), topic.size());
			ndn::Block keyBlock(128, buffer); // key

			string drnPrefix = m_drnPrefix.get(0).toUri();
			buffer = make_shared<::ndn::Buffer>((const void*)drnPrefix.c_str(), drnPrefix.size());
			ndn::Block valueBlock(129, buffer); // value

			ndn::Block params(ndn::tlv::Parameters);
			params.push_back(keyBlock);
			params.push_back(valueBlock);

			params.encode();

			interest->setParameters(params);

		    // send put interest
			m_appLink->onReceiveInterest(*interest);
			return;
		} else if(0 < std::get<0>(result)) {
			// 못 찾으면
			::ndn::Name name(successor.first);
			name.append("finger").append("finger");
			name.append(stringf("%d", std::get<1>(result)));
			name.append(topic);
			// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
			std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);

			interest->setName(name);
			interest->setCanBePrefix(true);
			interest->setMustBeFresh(true);
	//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
	//		interest->setInterestLifetime(interestLifeTime);

			// send put interest
			m_appLink->onReceiveInterest(*interest);
			return;
		} else {
			// 없다.
		}
	}
}

void RendezvousDHT::receiveInterestPA(const Name &interestName, int32_t attentionIndex, string topic) {
	pair<string, lli> successor;

	//  topic의 hash값을 계산하여
	lli topicHash = Helper::getHash(topic);

	// successor를 찾는다.
	std::tuple<int, int> result;
	result = m_dhtNode.findSuccessor(topicHash, successor);
	// successor를 못 찾으면 가까운 노드를 찾는다.
	if(std::get<0>(result) < 0) {
		result = m_dhtNode.closestPrecedingNode(topicHash, M, successor);
	}

	if(std::get<0>(result) == 0) {
		// 찾으면
		// /RN-{yyy}/PA/a/b/c/topic-{nnn}
		Name paInterestName(successor.first);
		paInterestName.append("PA");

		Name topicName = interestName.getSubName(2, Name::npos);
		paInterestName.append(topicName);

		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
		interest->setName(paInterestName);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

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
		ndn::Block params(ndn::tlv::Parameters, buffer);
#endif
		// 여러개는 보내기 전에 encoding을 해야 하고, 받아서는 parse를 해야 한다.
		params.encode();

		interest->setParameters(params);

		NS_LOG_DEBUG("send interest: " << paInterestName << ", topic prefix: " << topic << ", value: " << drnPrefix );

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else if(0 < std::get<0>(result)) {
		// 못 찾으면
		::ndn::Name name(successor.first);
		name.append("PA").append("finger");
		name.append(stringf("%d", std::get<1>(result)));
		name.append(topic);
		// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();

		interest->setName(name);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		// send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else {
		// 없다.
	}
}

void RendezvousDHT::sendInterestPA(const Name &interestName, int32_t attentionIndex, string nodeName) {
	// 찾으면
	// /RN-{yyy}/PA/a/b/c/topic-{nnn}
	Name paInterestName(nodeName);
	paInterestName.append("PA");

	Name topicName = interestName.getSubName(2, Name::npos);
	paInterestName.append(topicName);

	std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
	interest->setName(paInterestName);
	interest->setCanBePrefix(true);
	interest->setMustBeFresh(true);
//	time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//	interest->setInterestLifetime(interestLifeTime);

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
	ndn::Block params(ndn::tlv::Parameters, buffer);
#endif
	// 여러개는 보내기 전에 encoding을 해야 하고, 받아서는 parse를 해야 한다.
	params.encode();

	interest->setParameters(params);

	NS_LOG_DEBUG("send interest: " << paInterestName);

	// send put interest
	m_appLink->onReceiveInterest(*interest);
}

void RendezvousDHT::receiveInterestPU(const Name &interestName, string topic) {
	pair<string, lli> successor;

	lli topicHash = Helper::getHash(topic);

	// successor를 찾는다.
	std::tuple<int, int> result;
	result = m_dhtNode.findSuccessor(topicHash, successor);
	// successor를 못 찾으면 가까운 노드를 찾는다.
	if(std::get<0>(result) < 0) {
		result = m_dhtNode.closestPrecedingNode(topicHash, M, successor);
	}

	if(std::get<0>(result) == 0) {
		// 찾으면
		// /RN-{yyy}/PA/topic-{nnn}/...
		Name puInterestName(successor.first);
		puInterestName.append("PU");

		Name topicName = interestName.getSubName(2, Name::npos);
		puInterestName.append(topicName);

		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
		interest->setName(puInterestName);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);
//
//		ndn::tlv::AppPrivateBlock1 = 128;
//		ndn::tlv::AppPrivateBlock2 = 32767;
//		ndn::tlv::ContentType_Blob = 0;

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
		ndn::Block params(ndn::tlv::Parameters, buffer);
#endif
		params.encode();

		interest->setParameters(params);

		NS_LOG_DEBUG("send interest: " << puInterestName << ", topic prefix: " << topic << ", value: " << drnPrefix );

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else if(0 < std::get<0>(result)) {
		// 못 찾으면
		::ndn::Name name(successor.first);
		name.append("PU").append("finger");
		name.append(stringf("%d", std::get<1>(result)));
		name.append(topic);
		// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>(successor.first);

		interest->setName(name);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		// send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else {
		// 없다.
	}
}

void RendezvousDHT::receiveInterestPATopic(lli keyHash, string nodeName) {
	m_dhtNode.storeKey(keyHash, nodeName);
}

void RendezvousDHT::receiveInterestPUTopic(lli keyHash) {
	m_dhtNode.removeKey(keyHash);
}

void RendezvousDHT::receiveInterestRNTS(const Name &interestName, int32_t attentionIndex) {
	pair<string, lli> successor;

	// /RN/TS/a/b/c/topic-{nnn}에서 3번째를 추출("a")
	string topicPrefix = interestName.get(attentionIndex).toUri();

	//  topic의 hash값을 계산하여
	lli topicHash = Helper::getHash(topicPrefix);

	// successor를 찾는다.
	std::tuple<int, int> result;
	result = m_dhtNode.findSuccessor(topicHash, successor);
	// successor를 못 찾으면 가까운 노드를 찾는다.
	if(std::get<0>(result) < 0) {
		result = m_dhtNode.closestPrecedingNode(topicHash, M, successor);
	}

	if(std::get<0>(result) == 0) {
		// 찾으면
		// /RN-{yyy}/TS/a/b/c

		if(m_drnPrefix.equals(Name(successor.first))) {
		}

		Name tsInterestName(successor.first);
		tsInterestName.append("TS");

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		tsInterestName.append(topicName);

		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
		interest->setName(tsInterestName);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		NS_LOG_DEBUG("send interest: " << tsInterestName );

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else if(0 < std::get<0>(result)) {
		// 못 찾으면
		::ndn::Name name(successor.first);
		name.append("TS").append("finger");
		name.append(stringf("%d", std::get<1>(result)));
		name.append(topicPrefix);
		// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();

		interest->setName(name);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		// send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else {
		// 없다.
	}
}

void RendezvousDHT::receiveInterestTM(const Name &interestName, int32_t attentionIndex) {
	pair<string, lli> successor;

	// /RN/TS/a/b/c/topic-{nnn}에서 3번째를 추출("a")
	string topicPrefix = interestName.get(attentionIndex).toUri();

	//  topic의 hash값을 계산하여
	lli topicHash = Helper::getHash(topicPrefix);

	// successor를 찾는다.
	std::tuple<int, int> result;
	result = m_dhtNode.findSuccessor(topicHash, successor);
	// successor를 못 찾으면 가까운 노드를 찾는다.
	if(std::get<0>(result) < 0) {
		result = m_dhtNode.closestPrecedingNode(topicHash, M, successor);
	}

	if(std::get<0>(result) == 0) {
		// 찾으면
		// /RN-{yyy}/PA/a/b/c/topic-{nnn}
		Name tsInterestName(successor.first);
		tsInterestName.append("TM");

		Name topicName = interestName.getSubName(attentionIndex, Name::npos);
		tsInterestName.append(topicName);

		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();
		interest->setName(tsInterestName);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		NS_LOG_DEBUG("send interest: " << tsInterestName );

	    // send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else if(0 < std::get<0>(result)) {
		// 못 찾으면
		::ndn::Name name(successor.first);
		name.append("TM").append("finger");
		name.append(stringf("%d", std::get<1>(result)));
		name.append(topicPrefix);
		// TODO: successor를 못찾으면 여기서 더 구현해야 한다.
		std::shared_ptr<::ndn::Interest> interest = std::make_shared<::ndn::Interest>();

		interest->setName(name);
		interest->setCanBePrefix(true);
		interest->setMustBeFresh(true);
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

		// send put interest
		m_appLink->onReceiveInterest(*interest);
		return;
	} else {
		// 없다.
	}
}

void RendezvousDHT::sendInterestTM(const Name &interestName, int32_t attentionIndex, string nodeName) {
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
//		time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
//		interest->setInterestLifetime(interestLifeTime);

	NS_LOG_DEBUG("send interest: " << tmInterestName );

    // send put interest
	m_appLink->onReceiveInterest(*interest);
	return;
}
