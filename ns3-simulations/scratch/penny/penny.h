#ifndef PENNY_H
#define PENNY_H

#include "sim.h"
#include "libs/json/json.hpp"
#include "ns3/core-module.h"
using json = nlohmann::json;

#include "libs/intervalTree/intervalTree.h"
#define INTERVAL_TREE_SAFE_INTERVALS // makes sure that upper_bound >
                                     // lower_bound (by swapping if
                                     // neccessary), but is slower. Will become
                                     // an assert if left out.

struct pennyCounters
{
    uint64_t totalPkts = 0;     // All packets (mapped to a flowTest)
    uint64_t dataPkts = 0;      // Data packets
    uint64_t pureAckPkts = 0;   // Pure ACK packets
    uint64_t droppablePkts = 0; // Droppable packets
    uint64_t inOrderPkts = 0;
    uint64_t outOfOrderPkts = 0;           // Out-of-seq packets
    uint64_t droppedPkts = 0;              // Dropped packets
    uint64_t retransmittedDroppedPkts = 0; // Retransmitted dropped packets
    uint64_t notSeenDroppedPkts = 0;       // Not seen dropped packets
    uint64_t duplicatePkts = 0;            // Duplicate packets
    uint64_t pendingDroppedPkts = 0;       // Dropped packets with no decision yet
};

struct pennyMetaLists
{
    std::set<std::string> droppedPcksList;
    std::set<std::string> expiredPcksList;
    std::set<std::string> retransmittedPktsList;
};

struct statsSnapshot
{
    uint32_t highestSeq;
    std::string packetId;
    struct pennyCounters counters;
    struct pennyMetaLists lists;
};

struct pennyParameters
{
    double dropProbability = 0.0;
    double maxDuplicates = 0.0;
    double probabilityNotObserveRetransmission = 0.0;
    double packetDropExpirationTimeout = 0.0;
    int minPacketDrops = 0;
    int minDroppablePkts = 0;
    bool stopIndivFlowIfDecisionMade = false;
};

struct aggrCounterSnapshot
{
    std::string packetId;
    std::string flowId;

    uint64_t duplicatePktsAtDropInstance;

    struct pennyCounters counters;
    std::map<std::string, struct pennyMetaLists> lists;

    uint64_t flowsContributed = 0;
};

class penny
{
  public:
    /* Constructor */
    penny();

    void Enable();
    void Disable();

    bool isEnabled();
    bool isRunning();

    /* Check if Penny already tracks the flow. */
    bool isFlowTracked(std::string);

    /* Track new flow. */
    void trackNewFlow(std::string);

    /* Process a single packet (closedLoop or Spoofed). */
    int processPacket(struct simplePacket);

    /* Set the Penny and PennyFlow configuration. */
    void setConfiguration(json);

    /* Get the number of tracked closed-loop flows. */
    int getNumberOfTrackFlows();

    /* Pre-register spoofed flow. */
    void preregisterSpoofedFlow(std::string);

    json exportToJson(bool);

    json exportFlowCountersJson(struct pennyCounters);

    /* Track the number of packets per type */
    uint64_t totalClosedLoopPackets = 0;
    uint64_t totalSpoofedPackets = 0;

    std::set<std::string> indivFlowsClosedLoop;

    bool indivFlowsEnabled = false;

    std::string aggrOutcome;
    std::string finalOutcome;

  private:
    json conf;
    struct pennyParameters pennyParams;

    /* Map flows to pennyFlow instances */
    std::map<std::string, class pennyFlow> flowIdToPennyFlowMap;

    int evaluateAggrHypotheses(struct aggrCounterSnapshot);

    void checkAndUpdateExpired(struct aggrCounterSnapshot);

    void checkAndUpdateRetransmitted(struct aggrCounterSnapshot);

    void checkAndUpdateDuplicates(struct aggrCounterSnapshot);

    void addPacketDropSnapshot(struct simplePacket);

    std::set<std::string> activeClosedLoopFlows;
    std::set<std::string> activeSpoofedFlows;

    bool enabled = false, finished = false;

    std::list<struct aggrCounterSnapshot> pendingDropSnapsList;
    std::list<struct aggrCounterSnapshot> evaluatedSnapsList;

    std::set<std::string> pennyFlowsClosedLoopList;
    std::set<std::string> pennyFlowsSpoofedList;
    std::set<std::string> pennyFlowsDuplicatesExceededList;
};

class pennyFlow
{
  public:
    /* Constructor */
    pennyFlow();

    /* Process a new packet */
    int processPacket(struct simplePacket);

    /* Set PennyFlow configuration */
    void setConfiguration(struct pennyParameters);

    /* Check if a packet drop expired */
    bool checkPacketDropTimeouts();

    /* Evaluate the hypotheses. */
    int evaluateHypotheses();

    /* Get the state of the flow */
    struct statsSnapshot getFlowState();

    struct statsSnapshot getCurFlowState();

    json exportFlowStatsJson();

    json exportFlowCountersJson(struct statsSnapshot);

    void disablePacketDrops();

    void enablePacketDrops();

    struct pennyMetaLists metaLists;

    bool enabledPacketsDrops = true;

    /* Drop the packet. */
    bool dropPacket(uint32_t, std::string);

    uint64_t getDuplicatesByPacketDropId(std::string);

  private:
    /* The current highest seq number. */
    uint32_t highestSeq = 0;

    /* Penny internal parameters. */
    struct pennyParameters pennyParams;

    /* Current flow stats. */
    struct pennyCounters curCounters;

    /* List of all counter snapshots. */
    std::list<struct statsSnapshot> counterSnapsList;

    /* The most recent valid snapshot to use */
    struct statsSnapshot validCounterSnap;

    /* State of pennyFlow instance. */
    bool decisionMade = false;

    /* The decision type. */
    int decisionType = 0;

    std::map<std::string, double> pendingDropsTimeMap; // Store as key the packetId and as value the
                                                       // time that the packet was dropped
    std::map<std::string, bool> droppedPktsDecisionMap; // Store as key the packetId and as value if
                                                        // we have observed a retransmission

    uint32_t seqOfLastDroppedPacket = 0;

    /* Functions */

    bool dropMorePackets();

    /* The interval structure to track seq space. */
    lib_interval_tree::interval_tree_t<uint32_t> pktsIntervalTree;

    /* Check if the payload of the packet is unique. */
    bool isPayloadUnique(uint32_t, uint32_t);

    /* Add packet to the interval tree. */
    void addPktToSeqIntervalTree(uint32_t, uint32_t);

    /* Check if a more recent drop snapshot is now valid. */
    void checkForNewValidSnapshot();

    void addPacketDropSnapshot(std::string);

    void updateDropSnapshotsAheadExpired(std::string);

    void updateDropSnapshotsAheadRetransmitted(std::string);

    void updateDropSnapshotsAheadDuplicates(uint32_t);
};

#endif // PENNY_H