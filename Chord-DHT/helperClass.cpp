#include "headers.h"

#include <ns3/ndnSIM/ndn-cxx/util/logger.hpp>

#include "extensions/utils.hpp"
#include "helperClass.h"
#include "sha1.hpp"

/* splits the command into seperate arguments */
vector<string> Helper::splitCommand(string command){
    vector<string> arguments;
    int pos = 0;
    do{
        pos = command.find(' ');
        string arg = command.substr(0,pos); 
        arguments.push_back(arg);
        command = command.substr(pos+1);
    }while(pos != -1);
    return arguments;
}

#if 1
/* get SHA1 hash for a given key */
lli Helper::getHash(string key){
	char hex[SHA1_HEX_SIZE];
	char finalHash[41];
	string keyHash = "";
	size_t i;
	sha1 s;

	s.add(key.c_str(), key.size());
	s.finalize();
	s.print_hex(hex);

	lli mod = pow(2, M);

#if 0
	for (i = 0; i < M / 8; i++) {
		sprintf(finalHash, "%d", hex[i]);
		keyHash += finalHash;
	}
	lli hash = stoll(keyHash) % mod;
//	NS_LOG_UNCOND("1:" << keyHash << " -> " << hash << " " << stringf("%x", hash));
#else
	for (i = 0; i < M / 8; i++) {
		sprintf(finalHash, "%02x", hex[i]);
		keyHash += finalHash;
	}
	lli hash = strtoll(keyHash.c_str(), NULL, 16) % mod;
//	NS_LOG_UNCOND("2:" << keyHash << " -> " << hash << " " << stringf("%x", hash));
#endif

	return hash;
}
#else
/* get SHA1 hash for a given key */
lli Helper::getHash(string key){
    unsigned char obuf[41];
    char finalHash[41];
    string keyHash = "";
    size_t i;
    lli mod = pow(2,M);

    /* convert string to an unsigned char array because SHA1 takes unsigned char array as parameter */
    unsigned char unsigned_key[key.length()+1];
    for(i=0;i<key.length();i++){
        unsigned_key[i] = key[i];
    }
    unsigned_key[i] = '\0';


    SHA1(unsigned_key,sizeof(unsigned_key),obuf);
    for (i = 0; i < M/8; i++) {
        sprintf(finalHash,"%d",obuf[i]);
        keyHash += finalHash;
    }

    lli hash = stoll(keyHash) % mod;

    return hash;
}
#endif

/* keys and values are in form of key1:val1;key2:val2;.. , will seperate it accordingly */
vector< pair<lli,string> > Helper::seperateKeysAndValues(string keysAndValues){
    int size = keysAndValues.size();
    int i = 0;
    vector< pair<lli,string> > res;

    while(i < size){
        string key = "";
        while(i < size && keysAndValues[i] != ':'){
            key += keysAndValues[i];
            i++;
        }
        i++;

        string val = "";
        while(i < size && keysAndValues[i] != ';'){
            val += keysAndValues[i];
            i++;
        }
        i++;

        res.push_back(make_pair(stoll(key),val));
    }

    return res;
}

/* string is in form of ip:port;ip:port;... will seperate all these ip's and ports */
vector< string > Helper::seperateSuccessorList(string succList){
    int size = succList.size();
    int i = 0;
    vector< string > res;

    while(i < size){
        string nodeName = "";
        while(i < size && succList[i] != ';'){
        	nodeName += succList[i];
            i++;
        }
        i++;

        res.push_back(nodeName);
    }

    return res;
}

/* node receives all keys from it's predecessor who is leaving the ring */
void Helper::storeAllKeys(NodeInformation &nodeInfo,string keysAndValues){
    int pos = keysAndValues.find("storeKeys");

    vector< pair<lli,string> > res = seperateKeysAndValues(keysAndValues.substr(0,pos));

    for(size_t i=0;i<res.size();i++){
        nodeInfo.storeKey(res[i].first,res[i].second);
    }
}

/* send successor id of current node to the contacting node */
void Helper::sendSuccessorId(NodeInformation nodeInfo,int newSock,struct sockaddr_in client){

    pair<string, lli > succ = nodeInfo.getSuccessor();
    string succId = to_string(succ.second);
    char succIdChar[40];

    socklen_t l = sizeof(client);

    strcpy(succIdChar,succId.c_str());

    sendto(newSock,succIdChar,strlen(succIdChar),0,(struct sockaddr *)&client,l);

}

/* find successor of contacting node and send it's ip:port to it */
void Helper::sendSuccessor(NodeInformation nodeInfo,string nodeIdString,int newSock,struct sockaddr_in client){
    
//    lli nodeId = stoll(nodeIdString);

    socklen_t l = sizeof(client);
    
    /* find successor of the joining node */
    pair<string, lli > succNode;
//    int index = nodeInfo.findSuccessor(nodeId, succNode);

    /* get Ip and port of successor as ip:port in char array to send */
    char ipAndPort[40];
    string succName = succNode.first;
    strcpy(ipAndPort, succName.c_str());

    /* send ip and port info to the respective node */
    sendto(newSock, ipAndPort, strlen(ipAndPort), 0, (struct sockaddr*) &client, l);
}

void Helper::setTimer(struct timeval &timer){
    timer.tv_sec = 0;
    timer.tv_usec = 100000;
}

/* get predecessor node (ip:port) of the node having ip and port */
pair< string, lli > Helper::getPredecessorNode(string nodeName, string myNodeName, bool forStabilize){

#if 0
    struct sockaddr_in serverToConnectTo;
    socklen_t l = sizeof(serverToConnectTo);

    serverToConnectTo.sin_family = AF_INET;
    serverToConnectTo.sin_addr.s_addr = inet_addr(ip.c_str());
    serverToConnectTo.sin_port = htons(port);

    /* set timer for socket */
    struct timeval timer;
    setTimer(timer);

    int sock = socket(AF_INET,SOCK_DGRAM,0);

    if(sock < 0){
        perror("error");
        exit(-1);
    }

    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timer,sizeof(struct timeval));

    string msg = "";

    /* p2 means that just send predecessor of node ip:port , do not call notify */
    /* p1 means that this is for stabilize so notify node as well */

    if(forStabilize == true){
        msg = myNodeName;
        msg += "p1";
    }

    else
        msg = "p2";


    char ipAndPortChar[1024];
    strcpy(ipAndPortChar,msg.c_str());


    //  send to nodeName

    if (sendto(sock, ipAndPortChar, strlen(ipAndPortChar), 0, (struct sockaddr*) &serverToConnectTo, l) < 0){
        cout<<"yaha 12 "<<sock<<endl;
        perror("error");
        exit(-1);
    }


    int len = recvfrom(sock, ipAndPortChar, 1024, 0, (struct sockaddr *) &serverToConnectTo, &l);
    close(sock);

    if(len < 0){
        pair<string, lli > node;
        node.first = "";
        node.second = -1;
        return node;
    }

    ipAndPortChar[len] = '\0';

    string nodeName = ipAndPortChar;
//    lli hash;

    pair< string, lli > node;

    if(ipAndPort == "") {
        node.first = "";
        node.second = -1;
    } else {
        node.first = nodeName;
        node.second = getHash(nodeName);
    }
#else
    pair< string, lli > node;
	node.first = "";
	node.second = -1;
#endif

    return node;
}

/* combine successor list in form of ip1:port1;ip2:port2;.. */
string Helper::splitSuccessorList(vector< pair< string, lli > > list){
    string res = "";

    for(uint32_t i=0;i< list.size();i++){
        res = res + list[i].first + ";";
    }

    return res;
}

/* send ack to contacting node that this node is still alive */
void Helper::sendAcknowledgement(int newSock,struct sockaddr_in client){
    socklen_t l = sizeof(client);

    sendto(newSock,"1",1,0,(struct sockaddr*)&client,l);
}

/* check if node having ip and port is still alive or not */
bool Helper::isNodeAlive(string nodeName){
#if 0
    struct sockaddr_in serverToConnectTo;
    socklen_t l = sizeof(serverToConnectTo);

    serverToConnectTo.sin_family = AF_INET;
    serverToConnectTo.sin_addr.s_addr = inet_addr(ip.c_str());
    serverToConnectTo.sin_port = htons(port);

    /* set timer for socket */
    struct timeval timer;
    setTimer(timer);


    int sock = socket(AF_INET,SOCK_DGRAM,0);

    if(sock < 0){
        perror("error");
        exit(-1);
    }

    /* set timer on this socket */
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timer,sizeof(struct timeval));

    char msg[] = "alive";
    sendto(sock,msg,strlen(msg),0,(struct sockaddr *)&serverToConnectTo,l);

    char response[5];
    int len = recvfrom(sock,response,2,0,(struct sockaddr *)&serverToConnectTo,&l);

    close(sock);

    /* node is still active */
    if(len >= 0){
        return true;
    }
    else
        return false;
#else
    return true;
#endif
}

