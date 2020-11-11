
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/ndnSIM/ndn-cxx/util/logger.hpp"

#include "nodeInformation.hpp"

#include <iostream>

#include "headers.h"
#include "M.h"
#include "helperClass.h"

NS_LOG_COMPONENT_DEFINE("drn.NodeInformation");

using namespace std;

NodeInformation::NodeInformation(string nodeName, uint32_t maxNode)
	: m_id(0)
	, m_maxNode(maxNode)
	, m_nodeName(nodeName)
{
	m_fingerTable = vector< pair< string , lli > >(M+1);
	m_successorList = vector< pair< string , lli > >(m_maxNode+1);
	m_isInRing = false;
}

void NodeInformation::setStatus(){
	m_isInRing = true;
}

void NodeInformation::setDeadPredecessor() {
	if(m_predecessor.second == m_successor.second){
		m_successor.first = m_nodeName;
		m_successor.second = m_id;
		setSuccessorList(m_successor.first, m_successor.second);
	}
	m_predecessor.first = "";
	m_predecessor.second = -1;

}

void NodeInformation::updateSuccessor() {
	m_successor = m_successorList[2];
//	updateSuccessorList();
}

void NodeInformation::setSuccessor(string nodeName, lli hash){
	m_successor.first = nodeName;
	m_successor.second = hash;
}

void NodeInformation::setSuccessorList(string nodeName, lli hash){
	for(uint32_t i=1;i<=m_maxNode;i++){
		m_successorList[i] = make_pair(nodeName, hash);
	}
}

void NodeInformation::setPredecessor(string nodeName, lli hash){
	m_predecessor.first = nodeName;
	m_predecessor.second = hash;	
}

void NodeInformation::setId(lli nodeId){
	m_id = nodeId;
}

void NodeInformation::setFingerTable(string nodeName,lli hash){
	for(int i=1;i<=M;i++){
		m_fingerTable[i] = make_pair(nodeName, hash);
	}
}

void NodeInformation::setFingerTable(int index, string nodeName,lli hash){
	if(index < 1 || M < index) {
		return;
	}
	m_fingerTable[index] = make_pair(nodeName, hash);
}

void NodeInformation::storeKey(lli key,string val){
	m_dictionary[key] = val;
}

void NodeInformation::removeKey(lli key){
	map<lli, string>::iterator iter = m_dictionary.find(key);
	if(iter == m_dictionary.end()) {
		return;
	}
	m_dictionary.erase(iter);
}

void NodeInformation::printKeys(){
	map<lli,string>::iterator it;

	for(it = m_dictionary.begin(); it != m_dictionary.end() ; it++){
		cout<<it->first<<" "<<it->second<<endl;
	}
}

void NodeInformation::updateSuccessorList(vector< string > list){

	// 원래는 successor.first 로 요청하여 응답을 받아서 처리한다.
	// ndn 으로부터 받아서
	if(list.size() != m_maxNode) {
		return;
	}

	m_successorList[1] = m_successor;

	for(uint32_t i=2;i<=m_maxNode;i++){
		m_successorList[i].first = list[i-2];
		m_successorList[i].second = Helper::getHash(list[i-2]);
	}
}


/* send all keys of this node to it's successor after it leaves the ring */
vector< pair<lli , string> > NodeInformation::getAllKeysForSuccessor(){
	map<lli,string>::iterator it;
	vector< pair<lli , string> > res;

	for(it = m_dictionary.begin(); it != m_dictionary.end() ; it++){
		res.push_back(make_pair(it->first , it->second));
		m_dictionary.erase(it);
	}

	return res;
}

vector< pair<lli , string> > NodeInformation::getKeysForPredecessor(lli nodeId){
	map<lli,string>::iterator it;

	vector< pair<lli , string> > res;
	for(it = m_dictionary.begin(); it != m_dictionary.end() ; it++){
		lli keyId = it->first;

		/* if predecessor's id is more than current node's id */
		if(m_id < nodeId){
			if(keyId > m_id && keyId <= nodeId){
				res.push_back(make_pair(keyId , it->second));
				m_dictionary.erase(it);
			}
		}

		/* if predecessor's id is less than current node's id */
		else{
			if(keyId <= nodeId || keyId > m_id){
				res.push_back(make_pair(keyId , it->second));
				m_dictionary.erase(it);
			}
		}
	}

	return res;
}

tuple<int, int> NodeInformation::findSuccessor(lli nodeId, pair<string, lli> &successor){

//	pair < string, lli > self;
//	self.first = m_nodeName;
//	self.second = m_id;

	if(nodeId > m_id && nodeId <= m_successor.second) {
		successor = m_successor;
		return tuple<int, int>(0, 0);
	} else if(m_id == m_successor.second || nodeId == m_id) {
		successor.first = m_nodeName;
		successor.second = m_id;
		return tuple<int, int>(0, 0);
	} else if(m_successor.second == m_predecessor.second) {
		if(m_successor.second >= m_id) {
			if(nodeId > m_successor.second || nodeId < m_id) {
				successor.first = m_nodeName;
				successor.second = m_id;
				return tuple<int, int>(0, 0);
			}
		} else {
			if((nodeId > m_id && nodeId > m_successor.second) || (nodeId < m_id && nodeId < m_successor.second)) {
				successor = m_successor;
				return tuple<int, int>(0, 0);
			} else {
				successor.first = m_nodeName;
				successor.second = m_id;
				return tuple<int, int>(0, 0);
			}
		}
	}

	return tuple<int, int>(-1, -1);
}

tuple<int, int> NodeInformation::closestPrecedingNode(lli nodeId,  int index, pair<string, lli> &successor){
	for(int i=index;i>=1;i--){
		if(m_fingerTable[i].first == "" || m_fingerTable[i].second == -1){
			continue;
		}

		if(m_fingerTable[i].second > m_id && m_fingerTable[i].second < nodeId){
			successor = m_fingerTable[i];
			return tuple<int, int>(0, i);
		} else {
			successor = m_fingerTable[i];
			return tuple<int, int>(1, i);
		}
	}

	return tuple<int, int>(-1, -1);
}

tuple<int, int> NodeInformation::checkPredecessorNode(lli nodeId,  int index, pair<string, lli> &predecessor, pair<string, lli> &successor){
	int i = index;
	lli predecessorId = predecessor.second;

	if(predecessorId != -1 && m_fingerTable[i].second < predecessorId){
		if((nodeId <= m_fingerTable[i].second && nodeId <= predecessorId) || (nodeId >= m_fingerTable[i].second && nodeId >= predecessorId)){
			successor = predecessor;
			return tuple<int, int>(0, i);
		}
	}
	if(predecessorId != -1 && m_fingerTable[i].second > predecessorId && nodeId >= predecessorId && nodeId <= m_fingerTable[i].second){
		successor = predecessor;
		return tuple<int, int>(0, i);
	}

	return tuple<int, int>(-1, -1);
}

tuple<int, int> NodeInformation::checkSuccessorNode(lli nodeId,  int index, lli successorId, pair<string, lli> &successor){
	int i = index;
	if(successorId == -1) {
		return tuple<int, int>(-1, -1);
	}

	if(m_fingerTable[i].second > successorId){
		if((nodeId <= m_fingerTable[i].second && nodeId <= successorId) || (nodeId >= m_fingerTable[i].second && nodeId >= successorId)){
			successor = m_fingerTable[i];
			return tuple<int, int>(0, i);
		}
	}
	else if(m_fingerTable[i].second < successorId && nodeId > m_fingerTable[i].second && nodeId < successorId){
		successor = m_fingerTable[i];
		return tuple<int, int>(0, i);
	}

	successor = m_fingerTable[i];
	return tuple<int, int>(1, i);
}

void NodeInformation::stabilize(){

	/* get predecessor of successor */
	if(Helper::isNodeAlive(m_successor.first) == false)
		return;

	/* get predecessor of successor */
	pair<string, lli > predNode = Helper::getPredecessorNode(m_successor.first, m_nodeName, true);

	lli predecessorHash = predNode.second;

	if(predecessorHash == -1 || m_predecessor.second == -1)
		return;

	if(predecessorHash > m_id || (predecessorHash > m_id && predecessorHash < m_successor.second) || (predecessorHash < m_id && predecessorHash < m_successor.second)){
		m_successor = predNode;
	}
}

/* check if current node's predecessor is still alive */
std::tuple<int, int> NodeInformation::checkPredecessor(pair< string, lli > &xcessor){
	if(m_predecessor.second == -1) {
		return std::tuple<int, int>(-1, -1);
	}

	xcessor = m_predecessor;
	return std::tuple<int, int>(1, -1);

	if(Helper::isNodeAlive(m_predecessor.first) == false){
		/* if node has same successor and predecessor then set node as it's successor itself */
		if(m_predecessor.second == m_successor.second){
			m_successor.first = m_nodeName;;
			m_successor.second = m_id;
			setSuccessorList(m_successor.first, m_id);
		}
		m_predecessor.first = "";
		m_predecessor.second = -1;
	}
}

/* check if current node's successor is still alive */
std::tuple<int, int> NodeInformation::checkSuccessor(pair< string, lli > &xcessor){
	if(m_successor.second == m_id)
		return std::tuple<int, int>(-1, -1);

	xcessor = m_successor;
	return std::tuple<int, int>(1, -1);

	if(Helper::isNodeAlive(m_successor.first) == false){
		m_successor = m_successorList[2];

		vector<string> list;
		updateSuccessorList(list);
	}
}

void NodeInformation::notify(pair<string, lli > node){

	/* get id of node and predecessor */
//	lli predecessorHash = predecessor.second;
//	lli nodeHash = node.second;

	m_predecessor = node;

	/* if node's successor is node itself then set it's successor to this node */
	if(m_successor.second == m_id){
		m_successor = node;
	}
}

vector<pair<string, lli> > NodeInformation::getFingerTable(){
	return m_fingerTable;
}

lli NodeInformation::getId(){
	return m_id;
}

pair<string, lli > NodeInformation::getSuccessor(){
	return m_successor;
}

pair<string, lli > NodeInformation::getPredecessor(){
	return m_predecessor;
}

string NodeInformation::getValue(lli key){
	if(m_dictionary.find(key) != m_dictionary.end()){
		return m_dictionary[key];
	}
	else
		return "";
}

vector< pair<string, lli > > NodeInformation::getSuccessorList(){
	return m_successorList;
}

bool NodeInformation::getStatus(){
	return m_isInRing;
}
