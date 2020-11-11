/*
 * ndn-timeout-app.hpp
 *
 *  Created on: 2019. 11. 2.
 *      Author: root
 */

#ifndef EXTENSIONS_NDN_TIMEOUT_APP_HPP_
#define EXTENSIONS_NDN_TIMEOUT_APP_HPP_

#include <string>
#include <memory>
#include "ns3/core-module.h"

#include <ns3/ndnSIM/apps/ndn-app.hpp>
//#include <ns3/ndnSIM/ndn-cxx/name.hpp>
//#include <ns3/ndnSIM/ndn-cxx/face.hpp>
#include <ns3/ndnSIM/ndn-cxx/util/random.hpp>

#include "object-container.hpp"

#include "utils.hpp"

using namespace ns3;
using namespace ns3::ndn;
using namespace std;

class TimeoutApp : public App
{
public:
	static ns3::TypeId
	GetTypeId();

	TimeoutApp();
	
	virtual
	~TimeoutApp();
	
	virtual void
	StartApplication();
	
	virtual void
	StopApplication();

	virtual void
	OnInterest(shared_ptr<const Interest> interest);

	virtual void
	OnData(shared_ptr<const Data> data);

	virtual void
	OnNack(shared_ptr<const ::ndn::lp::Nack> nack);

	// process Interest timeout
	virtual void
	sendInterestTimeout(shared_ptr<const Interest> interest);

	virtual void
	OnTimeout(shared_ptr<const Interest> interest);
	
	void
	RemoveTimeoutEvent(const Name &name);
	
	// process Pending List
	virtual bool
	IsPendingTarget(const Name &name);
	virtual void
	OnPendingTimeout(shared_ptr<const Interest> interest);
	virtual void
	appendPendingTimeoutEvent(shared_ptr<const Interest> interest);
	virtual void
	removePendingTimeoutEvent(const Name &name);

protected:
	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>> m_timeoutEvent;

	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>> m_pendingEvent;
};


#endif /* EXTENSIONS_NDN_TIMEOUT_APP_HPP_ */
