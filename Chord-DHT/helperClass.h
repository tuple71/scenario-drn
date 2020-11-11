#ifndef helper_h
#define helper_h

#include <iostream>

#include "nodeInformation.hpp"

using namespace std;

typedef long long int lli;

class Helper {
public:
		static vector<string> splitCommand(string command);
		static vector< pair<lli,string> > seperateKeysAndValues(string keysAndValues);
		static vector< string > seperateSuccessorList(string succList);
		static string splitSuccessorList(vector<pair<string, lli>> list);
		
		static lli getHash(string key);
		
		static bool isNodeAlive(string nodeName);
		
		static void setTimer(struct timeval &timer);

		static void storeAllKeys(NodeInformation &nodeInfo,string keysAndValues);

		static pair<string, lli > getPredecessorNode(string nodeName, string myNodeName, bool forStabilize);

		static void sendSuccessor(NodeInformation nodeInfo,string nodeIdString,int newSock,struct sockaddr_in client);
		static void sendSuccessorId(NodeInformation nodeInfo,int newSock,struct sockaddr_in client);
		static void sendAcknowledgement(int newSock,struct sockaddr_in client);
};


#endif
