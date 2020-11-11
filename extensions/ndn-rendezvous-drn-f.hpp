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

#include "object-container.hpp"

#include "ndn-timeout-app.hpp"

#include "utils.hpp"

using namespace std;
using namespace ns3;
using namespace ns3::ndn;

class RendezvousDrnF : public TimeoutApp
{
public:
	static ns3::TypeId
	GetTypeId();

	RendezvousDrnF();

	virtual void
	StartApplication();

	virtual void
	StopApplication();

	int
	sendDataTM(const Name &interestName, int32_t attentionIndex);

	void
	sendDataPA(const Name &interestName, int32_t attentionIndex);

	virtual void
	OnInterestRN(shared_ptr<const Interest> interest);
	virtual void
	OnInterestRNXXX(shared_ptr<const Interest> interest);
	virtual void
	OnInterest(shared_ptr<const Interest> interest);
//	virtual void
//	OnPendingTimeout(shared_ptr<const Interest> interest);
//	virtual void
//	appendPendingTimeoutEvent(shared_ptr<const Interest> interest);
//	virtual void
//	removePendingTimeoutEvent(const Name &name);
	virtual void
	sendDataForPendingInsterestTM(const Name &topicName);
	virtual void
	sendDataForPendingInsterestDM(const Name &topicName);

	virtual void
	OnData(shared_ptr<const Data> data);

	virtual void
	OnNack(shared_ptr<const ::ndn::lp::Nack> nack);

	virtual bool
	IsPendingTarget(const Name &name);

	void sendData(const Name &dataName, const Block &content);
	void sendData(shared_ptr<const Interest> interest, const Block &content);

	void sendData(const Name &dataName, shared_ptr<const ::ndn::Buffer> &value);
	void sendData(shared_ptr<const Interest> interest, shared_ptr<const ::ndn::Buffer> &value);

	// /RN
	void receiveInterestRNPA(const Name &interestName, int32_t attentionIndex, string topic);
	void sendInterestRNXXXPA(const Name &interestName, int32_t attentionIndex, string nodeName);
	void receiveInterestRNPU(const Name &interestName, string topic);

	void receiveInterestRNTS(const Name &interestName, int32_t attentionIndex);
	void sendInterestRNXXXTM(const Name &interestName, int32_t attentionIndex, string nodeName);

	void receiveInterestRNTM(const Name &interestName, int32_t attentionIndex);

private:
	::ndn::Name m_rnPrefix;
	::ndn::Name m_drnPrefix;
	uint32_t m_nSub;
	uint32_t m_nDataSize;

	::ndn::random::RandomNumberEngine& m_rng;
	uint32_t m_appId;

	::ns3::Time m_interestLifeTime; ///< \brief LifeTime for interest packet

	uint32_t m_signature;
	Name m_keyLocator;

	// RN-00001/PA/a/b/c[RN-00002]
	std::map<std::string, std::string> m_PAMap;

	// RN/DP/a/b/c/topic-0
	std::map<std::string, BufferListPtr> m_DPMap;

	std::shared_ptr<std::vector<std::string>> m_dhtNodes;
	ns3::Ptr<ObjectContainer> m_objectContainer;

	uint32_t m_nRN__TSInterestCount;
	uint32_t m_nRNsnTSInterestCount;
	uint32_t m_nRNrvTSInterestCount;
	uint32_t m_nRN__TMInterestCount;
	uint32_t m_nRNsnTMInterestCount;
	uint32_t m_nRNrvTMInterestCount;
	uint32_t m_nRNrvDMInterestCount;
	uint32_t m_nRNrvDRInterestCount;

	uint32_t m_nRNsnTSNackCount;
	uint32_t m_nRNsnTMNackCount;

	uint32_t m_nRN__TSDataCount;
	uint32_t m_nRNsnTSDataCount;
	uint32_t m_nRNrvTSDataCount;
	uint32_t m_nRN__TMDataCount;
	uint32_t m_nRNsnTMDataCount;
	uint32_t m_nRNrvTMDataCount;
	uint32_t m_nRNsnDMDataCount;
	uint32_t m_nRNsnDRDataCount;

	uint32_t m_nRN__PAInterestCount;
	uint32_t m_nRNsnPAInterestCount;
	uint32_t m_nRNrvPAInterestCount;
	uint32_t m_nRN__PUInterestCount;
	uint32_t m_nRNsnPUInterestCount;
	uint32_t m_nRNrvPUInterestCount;
	uint32_t m_nRN__DPInterestCount;

	uint32_t m_nRNsnPANackCount;
	uint32_t m_nRNsnPUNackCount;

	uint32_t m_nRN__PADataCount;
	uint32_t m_nRNsnPADataCount;
	uint32_t m_nRNrvPADataCount;
	uint32_t m_nRN__PUDataCount;
	uint32_t m_nRNsnPUDataCount;
	uint32_t m_nRNrvPUDataCount;
	uint32_t m_nRN__DPDataCount;
};

#endif
