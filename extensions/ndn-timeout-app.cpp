#include "ndn-timeout-app.hpp"

#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

NS_LOG_COMPONENT_DEFINE("drn.TimeoutApp");

//using namespace ns3::ndn;
NS_OBJECT_ENSURE_REGISTERED(TimeoutApp);

ns3::TypeId
TimeoutApp::GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("TimeoutApp")
			.SetGroupName("Ndn")
			.SetParent<ns3::Object>()
			;

    return tid;
}

TimeoutApp::TimeoutApp() {
}

TimeoutApp::~TimeoutApp() {
};

void
TimeoutApp::StartApplication() {
	App::StartApplication();
}

void
TimeoutApp::StopApplication() {
	App::StopApplication();

	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>>::iterator timeoutIter;
	timeoutIter = m_timeoutEvent.begin();

	for (; timeoutIter != m_timeoutEvent.end(); timeoutIter ++) {
		Simulator::Remove(std::get<0>(timeoutIter->second));
	}
	m_timeoutEvent.clear();
}

void
TimeoutApp::OnInterest(shared_ptr<const Interest> interest) {
	App::OnInterest(interest);
}

void
TimeoutApp::OnData(shared_ptr<const Data> data) {
	App::OnData(data);

	NS_LOG_DEBUG(data->getName());

	RemoveTimeoutEvent(data->getName());
}

void
TimeoutApp::OnTimeout(shared_ptr<const Interest> interest) {
	NS_LOG_DEBUG(interest->getName());

	RemoveTimeoutEvent(interest->getName());
	sendInterestTimeout(interest);
}

void
TimeoutApp::OnNack(shared_ptr<const ::ndn::lp::Nack> nack) {
	App::OnNack(nack);

	NS_LOG_DEBUG(nack->getInterest().getName());

	RemoveTimeoutEvent(nack->getInterest().getName());
}

void
TimeoutApp::sendInterestTimeout(shared_ptr<const Interest> interest) {

	// 전송하고
	m_appLink->onReceiveInterest(*interest);

	time::milliseconds t = interest->getInterestLifetime();
	Time timeout = MilliSeconds(t.count());

	// Timer를 동작시킨다.
	std::shared_ptr<std::vector<EventId>> eventPtr = std::make_shared<std::vector<EventId>>();
	EventId eventId = Simulator::Schedule(timeout, &TimeoutApp::OnTimeout, this, interest);

	// interest name을 키로 (eventId, interest) 를 보관한다.
	// OnTimeout, OnData, OnNack 에서 timeoutEvent 삭제한다.
	std::tuple<::ns3::EventId, std::shared_ptr<const Interest>> tuple(eventId, interest);
	std::pair<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>> pair(interest->getName(), tuple);
	m_timeoutEvent.insert(pair);
}

void
TimeoutApp::RemoveTimeoutEvent(const Name &name) {
	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>>::iterator timeoutIter;
	timeoutIter = m_timeoutEvent.find(name);
	if (timeoutIter != m_timeoutEvent.end()) {
		Simulator::Remove(std::get<0>(timeoutIter->second));
		m_timeoutEvent.erase(timeoutIter);
	}
}

bool
TimeoutApp::IsPendingTarget(const Name &name) {
	return false;
}

/**
 * 타임 아웃된 interest는 삭제한다.
 */
void
TimeoutApp::OnPendingTimeout(shared_ptr<const Interest> interest) {
	NS_LOG_DEBUG(interest->getName());

	removePendingTimeoutEvent(interest->getName());
}

/**
 * Interest를 Pending한다.
 */
void
TimeoutApp::appendPendingTimeoutEvent(shared_ptr<const Interest> interest) {

	const Name &interestName = interest->getName();
	if(IsPendingTarget(interestName) == false) {
		return;
	}

	NS_LOG_DEBUG("Insert pending interest: " << interestName);

	time::milliseconds t = interest->getInterestLifetime();
	Time timeout = MilliSeconds(t.count());

	// Timer를 동작시킨다.
	std::shared_ptr<std::vector<EventId>> eventPtr = std::make_shared<std::vector<EventId>>();
	EventId eventId = Simulator::Schedule(timeout, &TimeoutApp::OnPendingTimeout, this, interest);

	std::tuple<::ns3::EventId, std::shared_ptr<const Interest>> tuple(eventId, interest);
	std::pair<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>> pair(interest->getName(), tuple);
	m_pendingEvent.insert(pair);
}

/**
 * pending된 interest를 삭제한다.
 */
void
TimeoutApp::removePendingTimeoutEvent(const Name &name) {
	std::map<::ndn::Name, std::tuple<::ns3::EventId, std::shared_ptr<const Interest>>>::iterator timeoutIter;
	timeoutIter = m_pendingEvent.find(name);
	if (timeoutIter != m_pendingEvent.end()) {
		NS_LOG_DEBUG("Erase pending interest: " << name);

		Simulator::Remove(std::get<0>(timeoutIter->second));
		m_pendingEvent.erase(timeoutIter);
	}
}

