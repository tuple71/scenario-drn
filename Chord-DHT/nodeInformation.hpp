#ifndef nodeInfo_h
#define nodeInfo_h

#include <iostream>
#include <vector>
#include <map>
#include <tuple>

#include "M.h"

using namespace std;

typedef long long int lli;

class NodeInformation{

private:
		lli m_id;
		pair< string, lli > m_predecessor;
		pair< string, lli > m_successor;
		vector< pair< string, lli > > m_fingerTable;
		map<lli, string> m_dictionary;
		vector< pair< string, lli > > m_successorList;

		bool m_isInRing;
		uint32_t m_maxNode;

	public:
		string m_nodeName;
//		SocketAndPort sp;
		
		NodeInformation(string nodeName, uint32_t maxNode);

		tuple<int, int> findSuccessor(lli nodeId, pair<string, lli> &successor);
		tuple<int, int> closestPrecedingNode(lli nodeId, int index, pair<string, lli> &successor);
		tuple<int, int> checkPredecessorNode(lli nodeId,  int index, pair<string, lli> &predecessor, pair<string, lli> &successor);
		tuple<int, int> checkSuccessorNode(lli nodeId, int index, lli successorId, pair<string, lli> &successor);
		void fixFingers();
		void stabilize();
		void notify(pair<string, lli > node);
		std::tuple<int, int> checkPredecessor(pair< string, lli > &xcessor);
		std::tuple<int, int> checkSuccessor(pair< string, lli > &xcessor);

		void updateSuccessorList(vector<string> list);

		void printKeys();
		void storeKey(lli key, string val);
		void removeKey(lli key);
		vector< pair<lli, string> > getAllKeysForSuccessor();
		vector< pair<lli, string> > getKeysForPredecessor(lli nodeId);

		void setSuccessor(string nodeName, lli hash);
		void setSuccessorList(string nodeName, lli hash);
		void setPredecessor(string nodeName, lli hash);
		void setFingerTable(string nodeName, lli hash);
		void setFingerTable(int index, string nodeName,lli hash);
		void setId(lli id);
		void setStatus();

		void setDeadPredecessor();
		void updateSuccessor();

		lli getId();
		string getValue(lli key);
		vector< pair< string, lli > > getFingerTable();
		pair< string, lli > getSuccessor();
		pair< string, lli > getPredecessor();
		vector< pair< string, lli > > getSuccessorList();
		bool getStatus();
};

#endif
