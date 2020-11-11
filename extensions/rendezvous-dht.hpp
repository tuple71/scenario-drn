/*
 * rendezvous.hpp
 *
 *  Created on: 2019. 9. 20.
 *      Author: root
 */

#ifndef EXTENSIONS_RENDEZVOUS_DHT_HPP_
#define EXTENSIONS_RENDEZVOUS_DHT_HPP_

#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

//#include <ns3/ndnSIM/helper/ndn-stack-helper.hpp>

#include <ns3/ndnSIM/model/ndn-common.hpp>
#include <ns3/ndnSIM/model/ndn-app-link-service.hpp>

#include "Chord-DHT/nodeInformation.hpp"

using namespace ns3::ndn;

class RendezvousDHT
{
public:
	static ns3::TypeId
	GetTypeId();

	RendezvousDHT(const ::ndn::Name& drnPrefix, const ::ndn::Name& predecessor, ns3::ndn::AppLinkService *appLink, int maxNode);

	virtual ~RendezvousDHT();

	void sendData(shared_ptr<const Interest> interest, const Block &content);

	void sendData(shared_ptr<const Interest> interest, shared_ptr<const ::ndn::Buffer> &value);


	// RN-{nnn}
	void create();
	void sendInterestJoin();
	void receiveInterestJoinSuccessor(shared_ptr<const Interest> interest, int32_t subcmdIndex);
	void receiveInterestJoinFinger(shared_ptr<const Interest> interest, int32_t subcmdIndex);
	void receiveInterestJoinGetKeys(shared_ptr<const Interest> interest, int32_t subcmdIndex);
	void receiveDataJoinSuccessor(shared_ptr<const Data> data, int32_t subcmdIndex);
	void receiveDataJoinFinger(shared_ptr<const Data> data, int32_t subcmdIndex);
	void receiveDataJoinGetKeys(shared_ptr<const Data> data, int32_t subcmdIndex);
	void sendInterestLeave();
	void receiveDataLeave(string successor);

	void receiveInterestStoreAllKeys(string keyAndValues);

	void doStabilize(int step);
	void sendInterestPredecessorAlive(string nodeName);
	void receiveDataPredecessorAlive(shared_ptr<const Data> data, int32_t subcmdIndex);
	void setDeadPredecessor();
	void sendInterestCheckSuccessorUpdateSuccessorList();

	void sendInterestSuccessorAlive(string nodeName);
	void receiveDataSuccessorAlive(shared_ptr<const Data> data, int32_t subcmdIndex);

	void receiveInterestSendSuccessorList(shared_ptr<const Interest> interest, int32_t cmd2ndIndex);
	void receiveDataSendSuccessorList(shared_ptr<const Data> data, int32_t cmd3thIndex);

	void sendInterestStabilizeAlive(string nodeName);
	void receiveDataStabilizeAlive(shared_ptr<const Data> data, int32_t cmd3thIndex);

	void sendInterestStabilizeP1(string nodeName);
	void receiveInterestStabilizeP1(shared_ptr<const Interest> interest, int32_t cmd2ndIndex);
	void receiveDataStabilizeP1(shared_ptr<const Data> data, int32_t cmd2ndIndex);

	void sendInterestUpdateSuccessorList();
	void receiveInterestUpdateSuccessorList(shared_ptr<const Data> data, int32_t cmdIndex);

	void sendInterestFixFingersAlive(string nodeName, int fingerIndex);
	void receiveDataFixFingersAlive(shared_ptr<const Data> data, int32_t cmdIndex);

	void sendInterestAlive(string nodeName, Name subCmd);
	void receiveDataAlive(string nodeName, string aliave);

	void sendInterestJoinGetKeys(string nodeName);

	void sendInterestGet(string key);
	string receiveInterestGet(string key);
	void receiveDataGet(string key, string value);

	void sendInterestPut(string key, string value);
	void receiveInterestPut(lli keyHash, string value);
	void receiveDataPut(string key);

	void receiveInterestP2(shared_ptr<const Interest> interest);
	void receiveDataP2(shared_ptr<const Data> data);
	// /RN
	void receiveInterestPA(const Name &interestName, int32_t attentionIndex, string topic);
	void sendInterestPA(const Name &interestName, int32_t attentionIndex, string nodeName);
	void receiveInterestPU(const Name &interestName, string topic);

	// /RN-{nnn}
	void receiveInterestPATopic(lli keyHash, string nodeName);
	void receiveInterestPUTopic(lli keyHash);

	void receiveInterestRNTS(const Name &interestName, int32_t attentionIndex);
	void sendInterestTM(const Name &interestName, int32_t attentionIndex, string nodeName);

	void receiveInterestTM(const Name &interestName, int32_t attentionIndex);

	::ndn::Name m_drnPrefix;
	::ndn::Name m_predecessor;
	ns3::ndn::AppLinkService *m_appLink;

	uint32_t m_signature;
	Name m_keyLocator;

	::ns3::EventId m_stablilizeEventId;

	NodeInformation m_dhtNode;
};

#endif /* EXTENSIONS_RENDEZVOUS_DHT_HPP_ */
