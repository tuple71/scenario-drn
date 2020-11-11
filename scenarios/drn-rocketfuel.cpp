/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

// push-simple.cpp
// tuple

#include <string>
#include <vector>
#include <tuple>
#include <regex>
#include <random>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

#include "ns3-dev/ns3/ndnSIM/utils/topology/rocketfuel-map-reader.hpp"

#include "extensions/utils.hpp"

namespace ns3 {

std::string g_topology = "topologies/bw-delay-rand-1/1239.r0-conv-annotated.txt";

std::string g_rendezvousIndexes("-#1");
std::string g_producerIndexes("-#1");
std::string g_consumerIndexes("-#1");

//uint64_t g_numberOfPublishMessages = std::numeric_limits<uint32_t>::max();
uint32_t g_numberOfSubscribeMessages = 100;
uint32_t g_numberOfDataStream = 200;
int g_simulationTime = 300;

string g_zPRandomize("none");
double g_fPFrequency = 1.0;

double g_nRStart = 3.0;
double g_nPStart = 5.0;
double g_nCStart = 8.0;

bool g_infoonly = false;

std::vector<std::tuple<int, int>> g_rendezvousIds;
std::vector<std::tuple<int, int>> g_producerIds;
std::vector<std::tuple<int, int>> g_consumerIds;

int g_rendezvousCount = 0;
int g_producerCount = 0;
int g_consumerCount = 0;

std::vector<std::tuple<int, int>> add_range(std::vector<std::tuple<int, int>> &nodes, std::smatch &ranges_match) {

	std::ssub_match id_match = ranges_match[1];
	std::string ids = id_match.str();
	if(0 < ids.size()) {
		int n = std::stoi(ids);
		nodes.push_back(std::tuple<int, int>(n, n));
	} else {
		std::ssub_match sid_match = ranges_match[3];
		std::ssub_match eid_match = ranges_match[4];
		std::string sids = sid_match.str();
		int sid = -1;
		int eid = -1;
		if(0 < sids.size()) {
			sid = std::stoi(sids);
		}
		std::string eids = eid_match.str();
		if(0 < eids.size()) {
			eid = std::stoi(eids);
		}

		nodes.push_back(std::tuple<int, int>(sid, eid));
	}

	return nodes;
}

int parse_indexes(std::vector<std::tuple<int, int>> &nodes, std::string &text) {
	const std::regex regex1("([^#]+)?(#([0-9]+))?");
	const std::regex regex2("([0-9]+)|(([0-9]+)?-([0-9]+)?)");

	int node_count = 0;

	std::smatch base_match;
	if (std::regex_match(text, base_match, regex1)) {
		if(base_match.size() == 4) {
			std::ssub_match index_match = base_match[1];
			std::string ranges = index_match.str();
			if(0 < ranges.size()) {
				std::string delimiter(",");
				size_t pos = 0;
				std::string token;
				while ((pos = ranges.find(delimiter)) != std::string::npos) {
				    token = ranges.substr(0, pos);
					std::smatch ranges_match;
					if (std::regex_match(token, ranges_match, regex2)) {
						if(ranges_match.size() == 5) {
							add_range(nodes, ranges_match);
						}
					}

				    ranges.erase(0, pos + delimiter.length());
				}
				std::smatch ranges_match;
				if (std::regex_match(ranges, ranges_match, regex2)) {
					if(ranges_match.size() == 5) {
						add_range(nodes, ranges_match);
					}
				} else {
					return -1;
				}
			} else {
				std::string defaultRange("-");
				std::smatch ranges_match;
				if (std::regex_match(defaultRange, ranges_match, regex2)) {
					if(ranges_match.size() == 5) {
						add_range(nodes, ranges_match);
					}
				} else {
					return -1;
				}
			}

			std::ssub_match count_match = base_match[3];
			std::string count = count_match.str();
			if(0 < count.size()) {
				node_count = std::stoi(count);
			}
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	return node_count;
}

int
selectNodes(std::vector<Ptr<Node>> &selectedNodes, std::vector<Ptr<Node>> &leafNode, std::vector<std::tuple<int, int>> &nodeIndexes) {
	std::vector<std::tuple<int, int>>::iterator iter;
	for(iter = nodeIndexes.begin(); iter != nodeIndexes.end(); iter ++) {
		int sIndex = std::get<0>(*iter);
		int eIndex = std::get<1>(*iter);

		if(sIndex < 0) {
			sIndex = 0;
		}
		if(eIndex < 0) {
			eIndex = leafNode.size()-1;
		}

		for(int index = sIndex; index <= eIndex; index += 1) {
			if(index == (int)leafNode.size()) {
				break;
			}

			selectedNodes.push_back(leafNode[index]);
		}
	}

	return selectedNodes.size();
}

/**
 * nodes의 노드 중 일부 또는 전체를 container에 추가한다.
 * selectCount > 0 면, selectCount 개수만큼 random 선택하여 추가한다.
 * 그렇지 않으면 전체를 추가한다.
 */
void
addToContainer(NodeContainer &container, std::vector<Ptr<Node>> &nodes, int selectCount) {
	// random
	std::random_device rd;
	std::mt19937 g(rd());

	if (0 < selectCount) {
		std::shuffle(nodes.begin(), nodes.end(), g);
	}

	for (int index = 0; index < (int)nodes.size(); index++) {
		if (0 < selectCount && index == selectCount) {
			break;
		}
		container.Add(nodes[index]);
	}
}

int parse_arguments(int argc, char *argv[]) {
	// Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
	CommandLine cmd;
	cmd.AddValue ("topology", "topology file path", g_topology);
	cmd.AddValue ("rendezvous", "rendezvous nodes(indexes of router node)", g_rendezvousIndexes);
	cmd.AddValue ("producer", "producer nodes(indexes of leaf node)", g_producerIndexes);
	cmd.AddValue ("consumer", "consumer nodes(indexes of leaf node)", g_consumerIndexes);
	cmd.AddValue ("sm", "Number of subscribe messages", g_numberOfSubscribeMessages);
//	cmd.AddValue ("pm", "Number of max publish messages", g_numberOfPublishMessages);
	cmd.AddValue ("ds", "Number of Data Stream", g_numberOfDataStream);
	cmd.AddValue ("duration", "Duration of simulation", g_simulationTime);
	cmd.AddValue ("p_random", "Type of send time randomization: none (default), uniform, exponential", g_zPRandomize);
	cmd.AddValue ("p_freq", "Frequency of topic generation / interest packets", g_fPFrequency);
	cmd.AddValue ("c_start", "consumer's start time", g_nCStart);
	cmd.AddValue ("p_start", "producer's start time", g_nPStart);
	cmd.AddValue ("r_start", "rendezvous's start time", g_nRStart);
	cmd.AddValue ("infoonly", "only print of topology node info.", g_infoonly);
	cmd.Parse(argc, argv);

	if (g_numberOfDataStream < g_numberOfSubscribeMessages) {
		g_numberOfSubscribeMessages = g_numberOfDataStream;
	}

	NS_LOG_UNCOND("program arguments:");
	NS_LOG_UNCOND("--topology      : " << g_topology);
	NS_LOG_UNCOND("--rendezvous    : " << g_rendezvousIndexes);
	NS_LOG_UNCOND("--producer      : " << g_producerIndexes);
	NS_LOG_UNCOND("--consumer      : " << g_consumerIndexes);
	NS_LOG_UNCOND("--sm            : " << g_numberOfSubscribeMessages);
//	NS_LOG_UNCOND("--pm            : " << g_numberOfPublishMessages);
	NS_LOG_UNCOND("--ds            : " << g_numberOfDataStream);
	NS_LOG_UNCOND("--duration      : " << g_simulationTime);
	NS_LOG_UNCOND("--p_random      : " << g_zPRandomize);
	NS_LOG_UNCOND("--p_freq        : " << g_fPFrequency);
	NS_LOG_UNCOND("--c_start       : " << g_nPStart);
	NS_LOG_UNCOND("--p_start       : " << g_nCStart);
	NS_LOG_UNCOND("--r_start       : " << g_nRStart);
	NS_LOG_UNCOND("--infoonly      : " << g_infoonly);

	if (0 < g_rendezvousIndexes.size()) {
		g_rendezvousCount = parse_indexes(g_rendezvousIds, g_rendezvousIndexes);
		if(g_producerCount < 0) {
			return -1;
		}
	}

	if (0 < g_producerIndexes.size()) {
		g_producerCount = parse_indexes(g_producerIds, g_producerIndexes);
		if(g_producerCount < 0) {
			return -1;
		}
	}

	if( 0 < g_consumerIndexes.size()) {
		g_consumerCount = parse_indexes(g_consumerIds, g_consumerIndexes);
		if(g_consumerCount < 0) {
			return -1;
		}
	}

	if (g_numberOfDataStream < g_numberOfSubscribeMessages) {
		g_numberOfSubscribeMessages = g_numberOfDataStream;
	}

	return 0;
}

int
main(int argc, char* argv[])
{
	int retval;

	// setting default parameters for PointToPoint links and channels
//	Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
//	Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
//	Config::SetDefault("ns3::QueueBase::MaxSize", StringValue("20p"));

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
	if ((retval = parse_arguments(argc, argv)) != 0) {
		return retval;
	}

    // Print Simulation Topology, Command Line Variables
    NS_LOG_UNCOND("simulation topology : " << g_topology);

    if(access(g_topology.c_str(), R_OK) != 0)
    {
    	NS_LOG_UNCOND(strerror(errno));
		Simulator::Destroy();
    	return 1;
    }

    // Read topologies and set parameters
	AnnotatedTopologyReader topologyReader("", 25);
	topologyReader.SetFileName(g_topology);
	NodeContainer nodeContainer = topologyReader.Read();

	// Install NDN stack on all nodes
	ndn::StackHelper ndnHelper;
	ndnHelper.SetDefaultRoutes(true);
	ndnHelper.InstallAll();

	std::string rnPrefix = "/RN";
	std::string drnPrefix = "/RN-%05d";
	std::string userPrefix = "topic";

	// Choosing forwarding strategy
	ndn::StrategyChoiceHelper::InstallAll(rnPrefix, "/localhost/nfd/strategy/best-route");

	// Installing global routing interface on all nodes
	ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
	ndnGlobalRoutingHelper.InstallAll();

	// Installing applications
	std::vector<Ptr<Node>> leafNode;
	std::vector<Ptr<Node>> bbNode;
	std::vector<Ptr<Node>> gwNode;

    // separate nodes(bb-, gw-, leaf-) and store
	NodeContainer::Iterator iter = nodeContainer.Begin();
	for (; iter != nodeContainer.End(); iter++) {
		std::string name = Names::FindName(*iter);
		if (name.compare(0, 3, "bb-") == 0) {
			bbNode.push_back(*iter);
		} else if (name.compare(0, 3, "gw-") == 0) {
			gwNode.push_back(*iter);
		} else if (name.compare(0, 5, "leaf-") == 0) {
			leafNode.push_back(*iter);
		}
	}

	// Rendezvous
	std::vector<Ptr<Node>> rendezvousNodes;
	NodeContainer rendezvousContainer;
	selectNodes(rendezvousNodes, leafNode, g_rendezvousIds);
	addToContainer(rendezvousContainer, rendezvousNodes, g_rendezvousCount);

	for (int index = 0; index < (int)rendezvousContainer.size(); index++) {
		Ptr<Node> node = rendezvousContainer[index];
		string name = Names::FindName(node);
		NS_LOG_UNCOND("Rendezvous-" << index << "\t: " << name << stringf("(%d)", node->GetId()));
	}

	// Producer
	std::vector<Ptr<Node>> producerNodes;
	NodeContainer producerContainer;
	selectNodes(producerNodes, leafNode, g_producerIds);
	addToContainer(producerContainer, producerNodes, g_producerCount);

	for (int index = 0; index < (int)producerContainer.size(); index++) {
		Ptr<Node> node = producerContainer[index];
		string name = Names::FindName(node);
		NS_LOG_UNCOND("Producer-" << index << "\t: " << name << stringf("(%d)", node->GetId()));
	}

	// Consumer
	std::vector<Ptr<Node>> consumerNodes;
	NodeContainer consumerContainer;
	selectNodes(consumerNodes, leafNode, g_consumerIds);
	addToContainer(consumerContainer, consumerNodes, g_consumerCount);

	for (int index = 0; index < (int)consumerContainer.size(); index++) {
		Ptr<Node> node = consumerContainer[index];
		string name = Names::FindName(node);
		NS_LOG_UNCOND("Consumer-" << index << "\t: " << name << stringf("(%d)", node->GetId()));
	}

	if(g_infoonly) {
		Simulator::Destroy();
		return 0;
	}

	uint32_t maxNode = rendezvousContainer.size();
	if(maxNode <  3) {
		maxNode = 3;
	}

	// Rendezvous
	ndn::AppHelper rendezvousHelper("RendezvousDrn");
	rendezvousHelper.SetAttribute("NumSubscribeMessage", UintegerValue(g_numberOfSubscribeMessages)); // 100 subs
	rendezvousHelper.SetAttribute("MaxNode", UintegerValue(maxNode));
//	consumerHelper.SetAttribute("TotalDataStream", UintegerValue(g_numberOfDataStream)); // 200 DS
	//consumerHelper.Install(nodes.Get(0)).Start(Seconds(10.0));

	std::string predecessor;
	int rnSeq = 0;
	NodeContainer::Iterator rendezvousIter = rendezvousContainer.Begin();
	for (; rendezvousIter != rendezvousContainer.End(); rendezvousIter++) {
		uint32_t nodeId = (*rendezvousIter)->GetId();
		string drnNodePrefix = stringf(drnPrefix, nodeId);
		if(rnSeq == 0) {
			predecessor = drnNodePrefix;
		}

		rendezvousHelper.SetAttribute("RnPrefix", StringValue(rnPrefix));
		rendezvousHelper.SetAttribute("DRnPrefix", StringValue(drnNodePrefix));
		rendezvousHelper.SetAttribute("Predecessor", StringValue(predecessor));

		NS_LOG_UNCOND(stringf("rnPrefix: %s, drnNodePrefix: %s, Predecessor: %s", rnPrefix.c_str(), drnNodePrefix.c_str(), predecessor.c_str()));

		ApplicationContainer container = rendezvousHelper.Install(*rendezvousIter);

		container.Start(Seconds(g_nRStart+rnSeq));
		container.Stop(Seconds(g_simulationTime-1.0));

//		ndnGlobalRoutingHelper.AddOrigins(drnNodePrefix, rendezvousContainer);

		rnSeq += 1;
	}

	// Producer
	ndn::AppHelper producerHelper("ProducerDrn");
	producerHelper.SetAttribute("RnPrefix", StringValue(rnPrefix));
	producerHelper.SetAttribute("TotalDataStream", UintegerValue(g_numberOfDataStream));
	producerHelper.SetAttribute("Randomize", StringValue(g_zPRandomize));
	producerHelper.SetAttribute("Frequency", DoubleValue(g_fPFrequency));
	//producerHelper.Install(nodes.Get(2)).Start(Seconds(2.0)); // last node

	//producerApp.Install(producerContainer).Start(Seconds(5));
	NodeContainer::Iterator producerIter = producerContainer.Begin();
	for (; producerIter != producerContainer.End(); producerIter++) {
		uint32_t nodeId = (*producerIter)->GetId();
		string nodePrefix = stringf("NODE-%05d", nodeId);

//		producerHelper.SetAttribute("TopicPrefix", StringValue("/a/b/c:/a/b/d"));
		producerHelper.SetAttribute("NodePrefix", StringValue(nodePrefix));
		producerHelper.SetAttribute("TopicPrefix", StringValue("/a/b/c"));

		ApplicationContainer container = producerHelper.Install(*producerIter);
		container.Start(Seconds(g_nPStart));
		container.Stop(Seconds(g_simulationTime-5.0));
	}

	// Consumer
	ndn::AppHelper consumerHelper("ConsumerDrn");
	consumerHelper.SetAttribute("RnPrefix", StringValue(rnPrefix));
	consumerHelper.SetAttribute("NumSubscribeMessage", UintegerValue(g_numberOfSubscribeMessages)); // 100 subs
	consumerHelper.SetAttribute("TotalDataStream", UintegerValue(g_numberOfDataStream)); // 200 DS
	//consumerHelper.Install(nodes.Get(0)).Start(Seconds(10.0));

	//consumerApp.Install(consumerContainer).Start(Seconds(8));
	NodeContainer::Iterator consumerIter = consumerContainer.Begin();
	for (; consumerIter != consumerContainer.End(); consumerIter++) {
		consumerHelper.SetAttribute("TopicPrefix", StringValue("/a/b/*"));

		ApplicationContainer container = consumerHelper.Install(*consumerIter);

		container.Start(Seconds(g_nCStart));
		container.Stop(Seconds(g_simulationTime-5.0));
	}

	rendezvousIter = rendezvousContainer.Begin();
	for (; rendezvousIter != rendezvousContainer.End(); rendezvousIter++) {
		Ptr<Node> node = (*rendezvousIter);
		string drnNodePrefix = stringf(drnPrefix, node->GetId());

		ndnGlobalRoutingHelper.AddOrigin(drnNodePrefix, node);
	}
	// Add /RN origins to ndn::GlobalRouter
	ndnGlobalRoutingHelper.AddOrigins(rnPrefix, rendezvousContainer);

	// Calculate and install FIBs
	ndn::GlobalRoutingHelper::CalculateRoutes();

	Simulator::Stop(Seconds(g_simulationTime));

	Simulator::Run();
	Simulator::Destroy();

	return 0;
}

} // namespace ns3

typedef long long int lli;

int
main(int argc, char* argv[]) {
	return ns3::main(argc, argv);
}
