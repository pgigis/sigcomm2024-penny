#include <filesystem>

#include "sim.h"
#include "penny.h"

using namespace ns3;

namespace fs = std::filesystem;

/* Global Variables */
penny pennyInstance;

bool ignoreLegitTraffic = false;
std::string closedLoopToSpoofedRatio;
double duplicationIllegit = 0.0;

/* Penny netDevice */
Ptr<PointToPointNetDevice> pennyDevice;

/* Next Seq for spoofed packet */
std::map<std::string, uint32_t> nextSeqSpoofedPacket;

/* Spoofed flows */
std::set<std::string> activeSpoofedFlows;

bool stopIfPennyFinishes = false;

void writeResults(const std::string& experimentFolder,
                  int argSeed,
                  double dropRate,
                  std::string topoId,
                  const json& data)
{
    try
    {
        fs::path resultsPath = "tempResults";
        fs::path experimentPath = resultsPath / experimentFolder;
        fs::path filePath = experimentPath / (topoId + "_" + std::to_string(dropRate) + "_" +
                                              std::to_string(argSeed) + ".txt");

        // Create directories if they do not exist
        fs::create_directories(experimentPath);

        // Open the file in append mode
        std::ofstream outfile(filePath, std::ios_base::app);
        if (!outfile)
        {
            throw std::ios_base::failure("Failed to open the file.");
        }

        outfile << data << std::endl; // Use data.dump(4) for pretty-printed JSON output
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error writing results: " << e.what() << std::endl;
    }
}

void processPacketNS3(Ptr<const Packet> packet)
{
    // Transform the NS-3 packet header to the custom SimplePacket format.
    struct simplePacket ns3Pkt = extractSimplePacket(packet);

    if (!ignoreLegitTraffic && ns3Pkt.synFlag)
    {
        if (!pennyInstance.isFlowTracked(ns3Pkt.flowId))
        {
            pennyInstance.trackNewFlow(ns3Pkt.flowId);
            return;
        }
    }
    std::list<struct simplePacket> pktsList;

    if (!ignoreLegitTraffic)
    {
        if (activeSpoofedFlows.size() > 0)
        {
            pktsList = generateSpoofedPackets(closedLoopToSpoofedRatio);
            int randomIndex =
                Random::get() % (pktsList.size() + 1); // "+1" to allow insertion at the end
            std::list<struct simplePacket>::iterator iter = pktsList.begin();
            std::advance(iter, randomIndex);
            pktsList.insert(iter, ns3Pkt);
        }
        else
        {
            pktsList.push_back(ns3Pkt);
        }
    }
    else
    {
        if (activeSpoofedFlows.size() > 0)
        {
            pktsList = generateSpoofedPackets(closedLoopToSpoofedRatio);
        }
    }

    /* Process the packets */
    for (const auto& pkt : pktsList)
    {
        if (pennyInstance.isRunning())
        {
            int pennyRetCode = pennyInstance.processPacket(pkt);

            if (pkt.isNS3Flow && pennyRetCode == 1)
            {
                // Drop the actual packet only for closed-loop ns-3 flows
                pennyDevice->SetDropPacketUid(packet->GetUid());
            }

            if (!pkt.isNS3Flow)
            {
                if (Random::get<bool>(duplicationIllegit))
                {
                    // Spoofed traffic: Duplicate the packet
                    pennyInstance.processPacket(pkt);
                }
            }
        }
    }
}

struct simplePacket extractSimplePacket(Ptr<const Packet> packet)
{
    struct simplePacket pkt = {0};
    TcpHeader* tcpH = (TcpHeader*)packet->extractTcpHeader();
    pkt.seq = tcpH->GetSequenceNumber().GetValue();
    pkt.ack = tcpH->GetAckNumber().GetValue();
    pkt.payloadSize = packet->GetPayloadSize();
    pkt.synFlag = tcpH->IsSYN();
    pkt.flowId =
        std::to_string(tcpH->GetSourcePort()) + "-" + std::to_string(tcpH->GetDestinationPort());
    pkt.packetId = std::to_string(pkt.seq) + "-" + std::to_string(pkt.ack);
    pkt.isNS3Flow = true;
    return pkt;
}

std::list<struct simplePacket> generateSpoofedPackets(std::string ratio)
{
    int spoofedPacketsNum = 0;
    if (ratio == "50:50")
    {
        spoofedPacketsNum = Random::get({0, 1, 2});
    }
    else if (ratio == "20:80")
    {
        spoofedPacketsNum = Random::get({3, 4, 5});
    }
    else if (ratio == "10:90")
    {
        spoofedPacketsNum = Random::get({7, 8, 9});
    }

    std::list<struct simplePacket> pktsList;

    for (int i = 0; i < spoofedPacketsNum; i++)
    {
        int randomIndex = Random::get() % activeSpoofedFlows.size();
        std::string flowId = *std::next(activeSpoofedFlows.begin(), randomIndex);

        struct simplePacket pkt = {0};
        pkt.seq = nextSeqSpoofedPacket[flowId];
        pkt.ack = 1;
        pkt.flowId = flowId;
        pkt.payloadSize = 1024;
        nextSeqSpoofedPacket[flowId] += 1024;
        pkt.synFlag = false;
        pkt.packetId = std::to_string(pkt.seq) + "-" + std::to_string(pkt.ack);
        pktsList.push_back(pkt);
    }
    return pktsList;
}

void InstallBulkSend(Ptr<Node> node,
                     Ipv4Address address,
                     uint16_t port,
                     std::string socketFactory,
                     uint32_t nodeId,
                     double startTimeWithOffset,
                     long int maxBytesToSend)
{
    BulkSendHelper source(socketFactory, InetSocketAddress(address, port));
    source.SetAttribute("MaxBytes", UintegerValue(maxBytesToSend));
    ApplicationContainer sourceApps = source.Install(node);
    sourceApps.Start(Seconds(startTimeWithOffset));
}

void InstallPacketSink(Ptr<Node> node, uint16_t port, std::string socketFactory, double startTime)
{
    PacketSinkHelper sink(socketFactory, InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(node);
    // sinker = StaticCast<PacketSink> (sinkApps.Get(0));
    sinkApps.Start(Seconds(startTime));
}

void BackgroundFlows(NodeContainer receiverNodeContainer,
                     NodeContainer senderNodeContainer,
                     Ipv4InterfaceContainer routerToReceiverIPAddress,
                     json config)
{
    int serverPort = 20000;

    int numberOfPackets = config["experiment"]["backgroundTraffic"]["numberOfPackets"].get<int>();
    uint64_t maxBytesToSend = 0;
    if (numberOfPackets != -1)
    {
        maxBytesToSend = 1024 * numberOfPackets;
    }

    for (int i = 0; i < config["experiment"]["backgroundTraffic"]["numberOfFlows"].get<int>(); i++)
    {
        // Install packet sink at receiver side
        InstallPacketSink(receiverNodeContainer.Get(0),
                          20000,
                          config["tcp"]["socketFactory"].get<std::string>(),
                          0.01);

        // Install BulkSend application
        InstallBulkSend(senderNodeContainer.Get(0),
                        routerToReceiverIPAddress.GetAddress(1),
                        20000,
                        config["tcp"]["socketFactory"].get<std::string>(),
                        senderNodeContainer.Get(0)->GetId(),
                        0.1,
                        maxBytesToSend);
        serverPort += 1;
    }
}

void PennyFlows(NodeContainer receiverNodeContainer,
                NodeContainer senderNodeContainer,
                Ipv4InterfaceContainer routerToReceiverIPAddress,
                json config)
{
    uint16_t server_port = 50000;

    int numberOfPackets = config["experiment"]["closedLoop"]["numberOfPackets"].get<int>();
    uint64_t maxBytesToSend = 0;
    if (numberOfPackets != -1)
    {
        maxBytesToSend = 1024 * numberOfPackets;
    }

    for (int i = 0; i < config["experiment"]["closedLoop"]["numberOfFlows"].get<int>(); i++)
    {
        double startTimeWithOffset = config["simulation"]["startTCPconn"].get<double>() +
                                     (double)Random::get<Random::common>(0.01, 0.2);
        // Install packet sink at receiver side
        InstallPacketSink(receiverNodeContainer.Get(0),
                          server_port,
                          config["tcp"]["socketFactory"].get<std::string>(),
                          startTimeWithOffset - 0.01);

        // Install BulkSend application
        InstallBulkSend(senderNodeContainer.Get(0),
                        routerToReceiverIPAddress.GetAddress(1),
                        server_port,
                        config["tcp"]["socketFactory"].get<std::string>(),
                        senderNodeContainer.Get(0)->GetId(),
                        startTimeWithOffset,
                        maxBytesToSend);
        server_port += 1;
    }
}

void pennyCallback(Ptr<const Packet> packet)
{
    /* Forward only PennyFlows */
    if (pennyInstance.isEnabled() && pennyInstance.isRunning())
    {
        /* Skip background Traffic */
        TcpHeader* pkt = (TcpHeader*)packet->extractTcpHeader();
        if ((pkt->GetSourcePort() >= 20000 && pkt->GetSourcePort() <= 21000) ||
            (pkt->GetDestinationPort() >= 20000 && pkt->GetDestinationPort() <= 21000))
            return;
        processPacketNS3(packet);
    }
    else
    {
        if (stopIfPennyFinishes)
        {
            Simulator::Stop(Simulator::Now() + Seconds(0.1));
        }
    }
}

int main(int argc, char* argv[])
{
    int argSeed = 0;
    std::string argExperimentConf = "", argTopologyConf = "", argPennyConf = "";

    CommandLine cmd;
    cmd.AddValue("argSeed", "Seed for randomness.", argSeed);
    cmd.AddValue("argExperimentConf", "The experiment configuration json file", argExperimentConf);
    cmd.AddValue("argTopologyConf", "The topology configuration json file", argTopologyConf);
    cmd.AddValue("argPennyConf", "The penny configuration json file", argPennyConf);
    cmd.Parse(argc, argv);

    if (argExperimentConf == "" || argTopologyConf == "" || argPennyConf == "")
    {
        std::cout << "Missing arguments." << std::endl;
        exit(-1);
    }

    /* Set random seed */
    Random::seed((int)argSeed);

    NodeContainer senderNodeContainer, receiverNodeContainer, routers;
    Ipv4InterfaceContainer senderToRouterIPAddress, routerToReceiverIPAddress;

    /* Read the configuration files */
    std::ifstream i("scratch/penny/configs/experiments/" + (std::string)argExperimentConf);
    json configData = json::parse(i);
    i.close();

    // std::cout << "Parsed experiments config" << std::endl;

    std::ifstream k("scratch/penny/configs/topology/" + (std::string)argTopologyConf);
    json confTopo = json::parse(k);
    k.close();

    // std::cout << "Parsed topology config" << std::endl;

    std::ifstream y("scratch/penny/configs/penny/" + (std::string)argPennyConf);
    json confPenny = json::parse(y);
    y.close();

    // std::cout << "Parsed penny config" << std::endl;

    /* Apply configs */
    ignoreLegitTraffic = configData["experiment"]["ignoreClosedLoop"]["enabled"].get<bool>();
    closedLoopToSpoofedRatio =
        configData["experiment"]["closedLoopToSpoofedRatio"].get<std::string>();

    stopIfPennyFinishes = configData["simulation"]["stopIfPennyFinishes"].get<bool>();

    if (configData["experiment"]["enablePenny"].get<bool>())
    {
        pennyInstance.Enable();
        pennyInstance.setConfiguration(confPenny);
    }

    if (configData["experiment"]["spoofedFlows"]["enabled"].get<bool>())
    {
        duplicationIllegit =
            configData["experiment"]["spoofedFlows"]["duplicationRate"].get<double>();
        /* Generate the spoofed flows */
        for (int i = 0; i < configData["experiment"]["spoofedFlows"]["numberOfFlows"].get<int>();
             i++)
        {
            std::string flowId = "SpoofedFlow-" + std::to_string(i);
            activeSpoofedFlows.insert(flowId);
            pennyInstance.preregisterSpoofedFlow(flowId);
        }
    }

    // Set TCP Recovery Algorithm
    Config::SetDefault(
        "ns3::TcpL4Protocol::RecoveryType",
        TypeIdValue(TypeId::LookupByName(configData["tcp"]["recoveryType"].get<std::string>())));

    // Set Congestion Control Algorithm
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue(configData["tcp"]["variantId"].get<std::string>()));

    // Set Delayed ACK Timeout
    Config::SetDefault("ns3::TcpSocket::DelAckTimeout",
                       TimeValue(Time(configData["tcp"]["delAckTimeout"].get<std::string>())));

    // Set Send and Receive Buffer Sizes
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    // Set Initial Congestion Window Size
    Config::SetDefault("ns3::TcpSocket::InitialCwnd",
                       UintegerValue(configData["tcp"]["initialCwnd"].get<int>()));

    // Set Delayed ACK Count
    Config::SetDefault("ns3::TcpSocket::DelAckCount",
                       UintegerValue(configData["tcp"]["delAckCount"].get<int>()));

    // Set TCP Segment Size
    Config::SetDefault("ns3::TcpSocket::SegmentSize",
                       UintegerValue(configData["tcp"]["segmentSize"].get<int>()));

    // Enable/Disable SACK in TCP
    Config::SetDefault("ns3::TcpSocketBase::Sack",
                       BooleanValue(configData["tcp"]["isSACKenabled"].get<bool>()));

    // Set Minimum RTO
    Config::SetDefault("ns3::TcpSocketBase::MinRto",
                       TimeValue(Seconds(configData["tcp"]["minRTO"].get<double>())));

    routers.Create(2);
    senderNodeContainer.Create(1);
    receiverNodeContainer.Create(1);

    /* Add aliases to the routers */
    Names::Add("Router1", routers.Get(0));
    Names::Add("Router2", routers.Get(1));
    Names::Add("Sender", senderNodeContainer.Get(0));
    Names::Add("Receiver", receiverNodeContainer.Get(0));

    /* Bottleneck link */
    DataRate b_bottleneck(confTopo["topology"]["bottleneck"]["bandwidth"].get<std::string>());
    Time d_bottleneck(confTopo["topology"]["bottleneck"]["delay"].get<std::string>());

    /* Access links */
    DataRate b_access(confTopo["topology"]["access"]["bandwidth"].get<std::string>());
    Time d_access(confTopo["topology"]["access"]["delay"].get<std::string>());

    /* Serilization delay */
    Time d_serialization(confTopo["topology"]["serializationDelay"].get<std::string>());

    /* Set DropTailQueue size to 1 packet */
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("1p"));

    /* Create the point-to-point link helpers and connect two router nodes */
    PointToPointHelper pointToPointRouter;
    pointToPointRouter.SetDeviceAttribute(
        "DataRate", StringValue(confTopo["topology"]["bottleneck"]["bandwidth"].get<std::string>()));
    pointToPointRouter.SetChannelAttribute(
        "Delay", StringValue(confTopo["topology"]["bottleneck"]["delay"].get<std::string>()));

    uint32_t max_bottleneck_bytes = static_cast<uint32_t>(
        (((double)std::min(b_access, b_bottleneck).GetBitRate() / 8) *
         (((d_access * 2) + d_bottleneck) * 2 + d_serialization).GetSeconds()));

    int maxQueuePackets = static_cast<int>(std::ceil(
        (double)max_bottleneck_bytes / configData["other"]["expectedPacketSize"].get<int>()));

    /* Create a net device container consisting ot router1 and router2 */
    NetDeviceContainer r1r2ND = pointToPointRouter.Install(routers.Get(0), routers.Get(1));

    /* Add aliases to the links */
    Names::Add("router1->router2", r1r2ND.Get(0));
    Names::Add("router2->router1", r1r2ND.Get(1));

    /* Create the point-to-point link helpers and connect send and receiver nodes to the routers */
    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute(
        "DataRate", StringValue(confTopo["topology"]["access"]["bandwidth"].get<std::string>()));
    pointToPointLeaf.SetChannelAttribute(
        "Delay", StringValue(confTopo["topology"]["access"]["delay"].get<std::string>()));

    NetDeviceContainer senderToRouter =
        pointToPointLeaf.Install(senderNodeContainer.Get(0), routers.Get(0));
    Names::Add("sender->router1", senderToRouter.Get(0));
    Names::Add("router1->sender", senderToRouter.Get(1));

    NetDeviceContainer routerToReceiver =
        pointToPointLeaf.Install(routers.Get(1), receiverNodeContainer.Get(0));
    Names::Add("router2->receiver", routerToReceiver.Get(0));
    Names::Add("receiver->router2", routerToReceiver.Get(1));

    /* Link losses */
    if (confTopo["topology"]["linkErrorRate"]["upstream"].get<double>() > 0)
    {
        Ptr<PointToPointNetDevice> ctr = senderToRouter.Get(0)->GetObject<PointToPointNetDevice>();
        ctr->EnableLinkLoss(confTopo["topology"]["linkErrorRate"]["upstream"].get<double>());
    }

    if (confTopo["topology"]["linkErrorRate"]["downstream"].get<double>() > 0)
    {
        // std::cout << "downstream loss" <<
        // confTopo["topology"]["linkErrorRate"]["downstream"].get<double>() <<
        // std::endl;
        Ptr<PointToPointNetDevice> rts =
            routerToReceiver.Get(0)->GetObject<PointToPointNetDevice>();
        rts->EnableLinkLoss(confTopo["topology"]["linkErrorRate"]["downstream"].get<double>());
    }

    InternetStackHelper internetStack;
    internetStack.Install(senderNodeContainer);
    internetStack.Install(receiverNodeContainer);
    internetStack.Install(routers);

    Ipv4AddressHelper ipAddresses("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer r1r2IPAddress = ipAddresses.Assign(r1r2ND);
    ipAddresses.NewNetwork();
    senderToRouterIPAddress = ipAddresses.Assign(senderToRouter);
    ipAddresses.NewNetwork();
    routerToReceiverIPAddress = ipAddresses.Assign(routerToReceiver);
    ipAddresses.NewNetwork();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    /* Set PfifoFastQueueDisc size */
    Config::SetDefault("ns3::PfifoFastQueueDisc::MaxSize",
                       QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, maxQueuePackets)));

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    QueueDiscContainer qd;
    tch.Uninstall(routers.Get(0)->GetDevice(0));
    qd.Add(tch.Install(routers.Get(0)->GetDevice(0)).Get(0));

    /* To be able to access packet metadata packet printing must be enabled */
    Packet::EnablePrinting();

    if (configData["experiment"]["enablePenny"].get<bool>())
    {
        r1r2ND.Get(0)->TraceConnectWithoutContext("MacTx", MakeCallback(&pennyCallback));
        pennyDevice = r1r2ND.Get(0)->GetObject<PointToPointNetDevice>();
        pennyDevice->EnablePacketDrop();
    }

    if (configData["experiment"]["backgroundTraffic"]["enabled"].get<bool>())
    {
        BackgroundFlows(
            receiverNodeContainer, senderNodeContainer, routerToReceiverIPAddress, configData);
    }

    if (configData["experiment"]["closedLoop"]["enabled"].get<bool>())
    {
        PennyFlows(
            receiverNodeContainer, senderNodeContainer, routerToReceiverIPAddress, configData);
    }

    if (configData["other"]["traces"]["enabled"].get<bool>())
    {
        pointToPointRouter.EnableAsciiAll("a");
    }

    Simulator::Stop(Seconds(configData["simulation"]["stopSimulation"].get<double>()));
    Simulator::Run();

    double dropRate = confPenny["penny"]["dropProbability"].get<double>();
    std::string topoId = confTopo["id"].get<std::string>();
    std::string folderName = configData["experiment"]["folder"].get<std::string>();

    writeResults(folderName, argSeed, dropRate, topoId, pennyInstance.exportToJson(false));

    Simulator::Destroy();
    return 0;
}