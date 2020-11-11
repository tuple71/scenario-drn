/*
 * rendezvous.hpp
 *
 *  Created on: 2019. 9. 20.
 *      Author: root
 */

#ifndef EXTENSIONS_OBJECT_CONTAINER_HPP_
#define EXTENSIONS_OBJECT_CONTAINER_HPP_

#include <memory>
#include <string>
#include <map>

#include <ns3/ndnSIM/model/ndn-common.hpp>
#include <boost/any.hpp>

class ObjectContainer : public ns3::Object
{
public:
	static ns3::TypeId
	GetTypeId();

	ObjectContainer();

	virtual ~ObjectContainer();
	template<class T>
	void set(std::string name, T &value) {
		m_dic.insert(std::pair<std::string, boost::any>(name, boost::any(value)));
	}

	template<class T>
	int get(std::string name, T &value) {
		std::map<std::string, boost::any>::iterator iter;
		iter = m_dic.find(name);
		if(iter == m_dic.end()) {
			return -1;
		}

		value = boost::any_cast<T>(iter->second);

		return 0;
	}

	std::map<std::string, boost::any> m_dic;
};

#endif /* EXTENSIONS_RENDEZVOUS_DHT_HPP_ */
