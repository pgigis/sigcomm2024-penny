#include "penny.h"

pennyFlow::pennyFlow() {}

int pennyFlow::processPacket(struct simplePacket pkt)
{
    /*
            Process Packet
            Return Codes:
             1 : Data packet dropped
    */

    curCounters.totalPkts++;

    if (pkt.payloadSize == 0)
    {
        curCounters.pureAckPkts++;
        return 0;
    }

    curCounters.dataPkts++;

    /* Check if the packet's payload overlaps with data we have seen in the past.
     */
    bool uniqPayload = isPayloadUnique(pkt.seq, pkt.payloadSize);

    /* Check if the packet has the highest SEQ number we have seen so far */
    if (pkt.seq < highestSeq && uniqPayload)
    {
        curCounters.outOfOrderPkts++;
        addPktToSeqIntervalTree(pkt.seq, pkt.payloadSize);
    }
    else
    {
        highestSeq = pkt.seq;
        curCounters.inOrderPkts++;
    }

    /* Check if any packet drops have expired. */
    checkPacketDropTimeouts();

    bool isDroppable = false;

    if (uniqPayload)
    {
        addPktToSeqIntervalTree(pkt.seq, pkt.payloadSize);
        curCounters.droppablePkts++;
        isDroppable = true;
    }
    else
    {
        if (droppedPktsDecisionMap.count(pkt.packetId) > 0 && !droppedPktsDecisionMap[pkt.packetId])
        {
            droppedPktsDecisionMap[pkt.packetId] = true;

            pendingDropsTimeMap.erase(pkt.packetId);
            curCounters.retransmittedDroppedPkts++;
            curCounters.pendingDroppedPkts--;
            updateDropSnapshotsAheadRetransmitted(pkt.packetId);
        }
        else
        {
            curCounters.duplicatePkts++;
            updateDropSnapshotsAheadDuplicates(pkt.seq);
        }
    }
    /* Update drop snapshots */
    checkForNewValidSnapshot();

    if (isDroppable)
    {
        return 1;
    }

    return 0;
}

bool pennyFlow::isPayloadUnique(uint32_t seq, uint32_t payloadSize)
{
    /*
        Interval boundaries are specified as follow:
        Starting point: the sequence number 
        Ending point: the sequence number + the payload size reduced by one
        # # #
        Note: In this lightweight implementation, we assume that retransmitted
        packets always fully compensate for the packet drop. For a complete
        implementation of this mechanism, please refer to the open-source
        Penny project. 
        # # #
    */
    if (pktsIntervalTree.overlap_find(
            {(unsigned int)seq, (unsigned int)seq + (unsigned int)(payloadSize - 1)}) ==
        std::end(pktsIntervalTree))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void pennyFlow::addPktToSeqIntervalTree(uint32_t seq, uint32_t payloadSize)
{
    pktsIntervalTree.insert(
        {(unsigned int)seq, (unsigned int)seq + (unsigned int)(payloadSize - 1)});
}

bool pennyFlow::checkPacketDropTimeouts()
{
    bool expiredPacket = false;
    double currentSimulationTime = ns3::Simulator::Now().GetSeconds();

    for (auto x = pendingDropsTimeMap.cbegin(), next_it = x; x != pendingDropsTimeMap.cend(); x = next_it)
    {
        ++next_it;

        double elapsedTime = currentSimulationTime - x->second;

        /* Case where the last drop has the highest SEQ number at this moment. */
        int seq =
            std::stoi((x->first).substr(0, (x->first).find("-"))); // Get seq from the packetId

        if (((int)seqOfLastDroppedPacket) == seq)
        {
            // Double the expiration timer
            elapsedTime = elapsedTime - pennyParams.packetDropExpirationTimeout;
        }

        if (elapsedTime > pennyParams.packetDropExpirationTimeout)
        {
            curCounters.pendingDroppedPkts--;
            curCounters.notSeenDroppedPkts++;
            droppedPktsDecisionMap[x->first] = true;
            updateDropSnapshotsAheadExpired(x->first);
            pendingDropsTimeMap.erase(x);
            expiredPacket = true;
        }
    }
    return expiredPacket;
}

void pennyFlow::updateDropSnapshotsAheadExpired(std::string packetId)
{
    bool modifySnapshotsAhead = false;
    std::list<struct statsSnapshot>::iterator iter;

    for (iter = counterSnapsList.begin(); iter != counterSnapsList.end(); ++iter)
    {
        if (iter->packetId == packetId)
        {
            iter->counters.notSeenDroppedPkts++; // Increase the non-observed packets counter
            iter->lists.expiredPcksList.insert(packetId);
            metaLists.expiredPcksList.insert(packetId);
            iter->counters.pendingDroppedPkts--; // Decrease the number of pending
                                                 // packets counter
            modifySnapshotsAhead = true;         // Modify dropSnapshot ahead
        }
        else
        {
            if (modifySnapshotsAhead)
            {
                iter->counters.notSeenDroppedPkts++; // Increase the non observed packets counter
                iter->lists.expiredPcksList.insert(packetId);
                iter->counters.pendingDroppedPkts--; // Decrease the number of pending packets
            }
        }
    }
}

void pennyFlow::updateDropSnapshotsAheadRetransmitted(std::string packetId)
{
    bool modifySnapshotsAhead = false;
    std::list<struct statsSnapshot>::iterator iter;

    for (iter = counterSnapsList.begin(); iter != counterSnapsList.end(); ++iter)
    {
        if (iter->packetId == packetId)
        {
            iter->counters.retransmittedDroppedPkts++; // Increase the observed packets counter
            iter->lists.retransmittedPktsList.insert(packetId);
            metaLists.retransmittedPktsList.insert(packetId);
            iter->counters.pendingDroppedPkts--; // Decrease the number of pending
                                                 // packets counter
            modifySnapshotsAhead = true;         // Modify dropSnapshot ahead
        }
        else
        {
            if (modifySnapshotsAhead)
            {
                iter->counters.retransmittedDroppedPkts++; // Increase the observed
                                                           // packets counter
                iter->lists.retransmittedPktsList.insert(packetId);
                iter->counters.pendingDroppedPkts--; // Decrease the number of pending packets
            }
        }
    }
    return;
}

void pennyFlow::updateDropSnapshotsAheadDuplicates(uint32_t seqDup)
{
    bool modifySnapshotsAhead = false;
    std::list<struct statsSnapshot>::iterator iter;

    for (iter = counterSnapsList.begin(); iter != counterSnapsList.end(); ++iter)
    {
        if (iter->highestSeq >= seqDup)
        {
            iter->counters.duplicatePkts++; // Increase the observed packets counter
            modifySnapshotsAhead = true;    // Modify dropSnapshot ahead
        }
        else
        {
            if (modifySnapshotsAhead)
            {
                iter->counters.duplicatePkts++; // Increase the observed packets counter
            }
        }
    }
}

uint64_t pennyFlow::getDuplicatesByPacketDropId(std::string packetId)
{
    std::list<struct statsSnapshot>::iterator iter;
    for (iter = counterSnapsList.begin(); iter != counterSnapsList.end(); ++iter)
    {
        if (iter->packetId == packetId)
        {
            return iter->counters.duplicatePkts;
        }
    }
    return -1;
}

void pennyFlow::disablePacketDrops()
{
    enabledPacketsDrops = false;
}

void pennyFlow::enablePacketDrops()
{
    enabledPacketsDrops = true;
}

bool pennyFlow::dropMorePackets()
{
    if (!enabledPacketsDrops)
    {
        return false;
    }

    if (pennyParams.minDroppablePkts > 0)
    {
        if (curCounters.droppablePkts >= (uint64_t)pennyParams.minDroppablePkts)
        {
            return false;
        }
    }

    if (pennyParams.minPacketDrops > 0)
    {
        if (curCounters.droppedPkts >= (uint64_t)pennyParams.minPacketDrops)
        {
            return false;
        }
    }

    return true;
}

bool pennyFlow::dropPacket(uint32_t seq, std::string packetId)
{
    /*
            Decide Whether to Drop the Packet
            Returns:
            True: If the packet was dropped.
            False: If the packet was not dropped.
    */
    if (Random::get<bool>(pennyParams.dropProbability) && dropMorePackets())
    {                                 // Randomly decide if we are going to drop the
                                      // packet
        seqOfLastDroppedPacket = seq; // Update the last dropped packet seq

        curCounters.droppedPkts++;        // Increase dropped packets counter
        curCounters.pendingDroppedPkts++; // Increase the number of pending packets

        droppedPktsDecisionMap[packetId] = false; // Add packet to the map of dropped packets
        pendingDropsTimeMap[packetId] =
            ns3::Simulator::Now().GetSeconds(); // Store the timestamp of the packet drop

        metaLists.droppedPcksList.insert(packetId); // Add packet to the list of dropped packets
        addPacketDropSnapshot(packetId);
        return true;
    }
    return false;
}

void pennyFlow::addPacketDropSnapshot(std::string packetId)
{
    struct statsSnapshot cs;
    cs.highestSeq = highestSeq;
    cs.packetId = packetId;
    cs.counters = curCounters;
    cs.lists = metaLists;
    counterSnapsList.push_back(cs);
}

void pennyFlow::checkForNewValidSnapshot()
{
    std::list<struct statsSnapshot>::iterator iter;
    for (iter = counterSnapsList.begin(); iter != counterSnapsList.end(); ++iter)
    {
        if (iter->counters.pendingDroppedPkts == 0)
        {
            validCounterSnap = *iter;
        }
    }
}

int pennyFlow::evaluateHypotheses()
{
    /*
            Return Codes:
            0: No decision
            1: Duplicates exceeded
            2: Closed-loop
            3: Non-bidirectional
    */

    struct statsSnapshot cs = getFlowState();

    if (cs.counters.retransmittedDroppedPkts == 0 && cs.counters.notSeenDroppedPkts == 0)
    {
        return 0;
    }

    if (pennyParams.minDroppablePkts > 0)
    {
        if (cs.counters.droppablePkts < (uint32_t)pennyParams.minDroppablePkts)
        {
            return 0;
        }
    }
    if (pennyParams.minPacketDrops > 0)
    {
        if (cs.counters.droppedPkts < (uint32_t)pennyParams.minPacketDrops)
        {
            return 0;
        }
    }

    double fDup = 0.0;
    int fDupNumerator = 0.0;
    int fDupDenominator = cs.counters.droppablePkts - cs.counters.droppedPkts;

    if (fDupDenominator < 1)
    {
        return 0;
    }

    if (cs.counters.duplicatePkts == 0)
    {
        fDupNumerator = 1.0;
    }
    else
    {
        fDupNumerator = cs.counters.duplicatePkts;
    }

    fDup = (double)((double)fDupNumerator / (double)fDupDenominator);

    if (fDup > pennyParams.maxDuplicates)
    {
        decisionType = 1;
        decisionMade = true;
        return 1;
    }

    double H1 = (double)pow((pennyParams.probabilityNotObserveRetransmission),
                            (double)cs.counters.notSeenDroppedPkts);
    double H2 = (double)pow(fDup, (double)cs.counters.retransmittedDroppedPkts);

    double closedLoopProbability = (double)((double)H1 / (double)(H1 + H2));

    if (closedLoopProbability > 0.99)
    {
        decisionType = 2;
        decisionMade = true;
        return 2;
    }
    else if (closedLoopProbability < 0.01)
    {
        decisionType = 3;
        decisionMade = true;
        return 3;
    }
    else
    {
        return 0;
    }
}

struct statsSnapshot pennyFlow::getCurFlowState()
{
    struct statsSnapshot tmpSnap;
    tmpSnap.counters = curCounters;
    tmpSnap.lists = metaLists;
    return tmpSnap;
}

struct statsSnapshot pennyFlow::getFlowState()
{
    if (curCounters.droppedPkts == 0)
    {
        /* Case where we haven't dropped any packets from this flow. */
        struct statsSnapshot tmpSnap;
        tmpSnap.counters = curCounters;
        tmpSnap.lists = metaLists;
        return tmpSnap;
    }
    else if (curCounters.notSeenDroppedPkts > 0 || curCounters.retransmittedDroppedPkts > 0)
    {
        /* Case where we have made a decision for at least one drop. */
        return validCounterSnap;
    }
    else
    {
        /* Returns the counter values up to the last packet drop. */
        return counterSnapsList.front();
    }
}

json pennyFlow::exportFlowCountersJson(struct statsSnapshot cs)
{
    json exportData;

    exportData["totalPkts"] = cs.counters.totalPkts;
    exportData["dataPkts"] = cs.counters.dataPkts;
    exportData["pureAckPkts"] = cs.counters.pureAckPkts;
    exportData["droppablePkts"] = cs.counters.droppablePkts;
    exportData["inOrderPkts"] = cs.counters.inOrderPkts;
    exportData["outOfOrderPkts"] = cs.counters.outOfOrderPkts;
    exportData["droppedPkts"] = cs.counters.droppedPkts;
    exportData["retransmittedDroppedPkts"] = cs.counters.retransmittedDroppedPkts;
    exportData["notSeenDroppedPkts"] = cs.counters.notSeenDroppedPkts;
    exportData["duplicatePkts"] = cs.counters.duplicatePkts;
    exportData["pendingDroppedPkts"] = cs.counters.pendingDroppedPkts;

    for (auto it = cs.lists.droppedPcksList.begin(); it != cs.lists.droppedPcksList.end(); ++it)
    {
        exportData["droppedPcksList"] += *it;
    }
    for (auto it = cs.lists.expiredPcksList.begin(); it != cs.lists.expiredPcksList.end(); ++it)
    {
        exportData["expiredPcksList"] += *it;
    }
    for (auto it = cs.lists.retransmittedPktsList.begin();
         it != cs.lists.retransmittedPktsList.end();
         ++it)
    {
        exportData["retransmittedPktsList"] += *it;
    }
    return exportData;
}

json pennyFlow::exportFlowStatsJson()
{
    json exportData;

    /* Export current values */
    struct statsSnapshot cur;

    cur.counters = curCounters;
    cur.lists = metaLists;

    exportData["current"] = exportFlowCountersJson(cur);

    int index = 0;
    for (std::list<struct statsSnapshot>::iterator iter = counterSnapsList.begin(); iter != counterSnapsList.end(); ++iter)
    {
        exportData["snapshots"][index] = exportFlowCountersJson(*iter);
        index++;
    }
    return exportData;
}

void pennyFlow::setConfiguration(struct pennyParameters value)
{
    pennyParams = value;
}