/*
 * consumer.cpp
 *
 *  Created on: 2019. 9. 20.
 *      Author: root
 */

#include "object-container.hpp"

#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

NS_LOG_COMPONENT_DEFINE("drn.ObjectContainer");

//using namespace ns3::ndn;
NS_OBJECT_ENSURE_REGISTERED(ObjectContainer);

ns3::TypeId
ObjectContainer::GetTypeId() {
    static ns3::TypeId tid = ns3::TypeId("ObjectContainer")
			.SetGroupName("Ndn")
			.SetParent<ns3::Object>()
			;

    return tid;
}

ObjectContainer::ObjectContainer() {
}

ObjectContainer::~ObjectContainer() {

}
