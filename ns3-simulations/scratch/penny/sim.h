#ifndef CUSTOM_H
#define CUSTOM_H

#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>

#include "libs/json/json.hpp"
#include "libs/random/random.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/queue-disc.h"
#include "ns3/traffic-control-module.h"

using Random = effolkronium::random_static;
using json = nlohmann::json;

using namespace ns3;

// Define the simplePacket struct
struct simplePacket
{
    uint32_t seq = 0;
    uint32_t ack = 0;
    uint32_t payloadSize = 0;
    std::string flowId = "";
    std::string packetId = "";
    bool synFlag = false;
    bool isNS3Flow = false;
};

// Function declarations
struct simplePacket extractSimplePacket(Ptr<const Packet>);
std::list<struct simplePacket> generateSpoofedPackets(const std::string fileName);
void processPacketNS3(Ptr<const Packet> packet);

#endif // CUSTOM_H