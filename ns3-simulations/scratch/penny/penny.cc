#include "penny.h"

penny::penny() {}

void penny::Enable()
{
    enabled = true;
}

bool penny::isEnabled()
{
    return enabled;
}

bool penny::isRunning()
{
    return !finished;
}

void penny::Disable()
{
    enabled = false;
}

void penny::setConfiguration(json confPenny)
{
    conf = confPenny;
    pennyParams.dropProbability = conf["penny"]["dropProbability"].get<double>();
    pennyParams.maxDuplicates = conf["penny"]["maxDuplicates"].get<double>();
    pennyParams.probabilityNotObserveRetransmission = conf["penny"]["probabilityNotObserveRetransmission"].get<double>();
    pennyParams.packetDropExpirationTimeout = conf["penny"]["timeouts"]["dropExpiration"].get<double>();
    pennyParams.minPacketDrops = conf["penny"]["execution"]["minPacketDrops"].get<int>();
    pennyParams.minDroppablePkts = conf["penny"]["execution"]["minDroppablePkts"].get<int>();
}

void penny::preregisterSpoofedFlow(std::string flowId)
{
    pennyFlow newPennyFlowInstance;
    flowIdToPennyFlowMap[flowId] = newPennyFlowInstance;
    flowIdToPennyFlowMap[flowId].setConfiguration(pennyParams);
}

int penny::processPacket(struct simplePacket pkt)
{
    /* Track the number of packets per type */
    (pkt.isNS3Flow ? totalClosedLoopPackets++ : totalSpoofedPackets++);

    /* Process packet in the individual flow instance. */
    int retCodeProcessPacket = flowIdToPennyFlowMap[pkt.flowId].processPacket(pkt);

    /* Check aggregate drop snapshots. */
    if (pendingDropSnapsList.size() > 0 && !indivFlowsEnabled)
    {
        struct aggrCounterSnapshot acs = pendingDropSnapsList.front();
        checkAndUpdateExpired(acs);
        checkAndUpdateRetransmitted(acs);
        checkAndUpdateDuplicates(acs);

        if (acs.counters.pendingDroppedPkts == 0)
        {
            acs = pendingDropSnapsList.front();
            pendingDropSnapsList.pop_front();
            evaluatedSnapsList.push_back(acs);

            /* Evaluate Aggregates */
            int aggrEvalOutcome = evaluateAggrHypotheses(acs);

            if (aggrEvalOutcome == 3)
            {
                aggrOutcome = "Not Closed-Loop";
                indivFlowsEnabled = true;
            }
            else if (aggrEvalOutcome == 2)
            {
                /* If aggregates closed-loop, Penny finishes. */
                finished = true;
                aggrOutcome = "Closed-Loop";
                finalOutcome = aggrOutcome;
            }
            else if (aggrEvalOutcome == 1)
            {
                /* If aggregates duplicates exceeded, Penny finishes. */
                finished = true;
                aggrOutcome = "Duplicates Exceeded";
                finalOutcome = aggrOutcome;
            }
        }
    }

    /* If aggregates not closed-loop, examine-individual flows. */
    if (indivFlowsEnabled)
    {
        if ((int)indivFlowsClosedLoop.size() >
            conf["penny"]["execution"]["minClosedLoopFlows"].get<int>())
        {
            finished = true;
            finalOutcome = "Closed-loop";
        }
    }

    if (!finished)
    {
        int retCodeEvaluate = flowIdToPennyFlowMap[pkt.flowId].evaluateHypotheses();
        if (retCodeEvaluate == 0)
        {
            /* No decision */
            if (retCodeProcessPacket == 1)
            {
                /*
                        If the number of dropped packets exceeds maxPacketDrops, disable
                   packet drops. Packet drops will be re-enabled only if the aggregates
                   reach a decision not closed-loop decision.
                */
                if (pendingDropSnapsList.size() + evaluatedSnapsList.size() <
                        conf["penny"]["execution"]["maxPacketDrops"] ||
                    indivFlowsEnabled)
                {
                    if (flowIdToPennyFlowMap[pkt.flowId].dropPacket(pkt.seq, pkt.packetId))
                    {
                        if (!indivFlowsEnabled)
                        {
                            addPacketDropSnapshot(pkt);
                        }
                        return 1;
                    }
                }
            }
            else if (retCodeEvaluate == 2)
            {
                indivFlowsClosedLoop.insert(pkt.flowId);
            }
        }
    }
    return 0;
}

void penny::checkAndUpdateExpired(struct aggrCounterSnapshot acs)
{
    if (flowIdToPennyFlowMap[acs.flowId].metaLists.expiredPcksList.count(acs.packetId) == 1)
    {
        // Packet expired - Update instances ahead
        for (std::list<struct aggrCounterSnapshot>::iterator iter = pendingDropSnapsList.begin(); iter != pendingDropSnapsList.end(); ++iter)
        {
            if (iter->lists[acs.flowId].expiredPcksList.count(acs.packetId) == 0 &&
                iter->lists[acs.flowId].droppedPcksList.count(acs.packetId) == 1)
            {
                iter->counters.notSeenDroppedPkts++;
                iter->counters.pendingDroppedPkts--;
                iter->lists[acs.flowId].expiredPcksList.insert(acs.packetId);
            }
        }
    }
}

void penny::checkAndUpdateRetransmitted(struct aggrCounterSnapshot acs)
{
    if (flowIdToPennyFlowMap[acs.flowId].metaLists.retransmittedPktsList.count(acs.packetId) == 1)
    {
        for (std::list<struct aggrCounterSnapshot>::iterator iter = pendingDropSnapsList.begin(); iter != pendingDropSnapsList.end(); ++iter)
        {
            // Update packet drops that are not aware of the packet expiration
            if (iter->lists[acs.flowId].retransmittedPktsList.count(acs.packetId) == 0 &&
                iter->lists[acs.flowId].droppedPcksList.count(acs.packetId) == 1)
            {
                iter->counters.retransmittedDroppedPkts++;
                iter->counters.pendingDroppedPkts--;
                iter->lists[acs.flowId].retransmittedPktsList.insert(acs.packetId);
            }
        }
    }
}

void penny::checkAndUpdateDuplicates(struct aggrCounterSnapshot acs)
{
    uint64_t duplicates =
        flowIdToPennyFlowMap[acs.flowId].getDuplicatesByPacketDropId(acs.packetId);
    if (duplicates > acs.duplicatePktsAtDropInstance)
    {
        for (std::list<struct aggrCounterSnapshot>::iterator iter = pendingDropSnapsList.begin(); iter != pendingDropSnapsList.end(); ++iter)
        {
            if (iter->packetId == acs.packetId)
            {
                iter->duplicatePktsAtDropInstance++;
            }
            // Update packet drops that are not aware of the packet expiration
            if (iter->lists[acs.flowId].retransmittedPktsList.count(acs.packetId) == 0 &&
                iter->lists[acs.flowId].expiredPcksList.count(acs.packetId) == 0 &&
                iter->lists[acs.flowId].droppedPcksList.count(acs.packetId) == 1)
            {
                iter->counters.duplicatePkts++;
            }
        }
    }
}

int penny::evaluateAggrHypotheses(struct aggrCounterSnapshot acs)
{
    /*
            Return Codes:
            0: No decision
            1: Duplicates exceeded
            2: Closed-loop
            3: Spoofed
    */
    if (pennyParams.minDroppablePkts > 0)
    {
        if (acs.counters.droppablePkts < (uint32_t)pennyParams.minDroppablePkts)
        {
            return 0;
        }
    }
    if (pennyParams.minPacketDrops > 0)
    {
        if (acs.counters.droppedPkts < (uint32_t)pennyParams.minPacketDrops)
        {
            return 0;
        }
    }

    double fDup = 0.0;
    int fDupNumerator = 0.0;
    int fDupDenominator = acs.counters.droppablePkts - acs.counters.droppedPkts;

    if (fDupDenominator < 1)
    {
        return 0;
    }

    if (acs.counters.duplicatePkts == 0)
    {
        fDupNumerator = 1.0;
    }
    else
    {
        fDupNumerator = acs.counters.duplicatePkts;
    }

    fDup = (double)((double)fDupNumerator / (double)fDupDenominator);

    if (fDup > pennyParams.maxDuplicates)
    {
        return 1;
    }

    double H1 = (double)pow(pennyParams.probabilityNotObserveRetransmission,
                            (double)acs.counters.notSeenDroppedPkts);
    double H2 = (double)pow(fDup, (double)acs.counters.retransmittedDroppedPkts);

    double closedLoopProbability = (double)((double)H1 / (double)(H1 + H2));

    if (closedLoopProbability > 0.99)
    {
        return 2;
    }
    else if (closedLoopProbability < 0.01)
    {
        return 3;
    }
    else
    {
        return 0;
    }
}

bool penny::isFlowTracked(std::string flowId)
{
    if (flowIdToPennyFlowMap.count(flowId) != 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void penny::trackNewFlow(std::string flowId)
{
    pennyFlow newPennyFlowInstance;
    flowIdToPennyFlowMap[flowId] = newPennyFlowInstance;
    flowIdToPennyFlowMap[flowId].setConfiguration(pennyParams);
}

void penny::addPacketDropSnapshot(struct simplePacket pkt)
{
    struct aggrCounterSnapshot acs;

    acs.packetId = pkt.packetId;
    acs.flowId = pkt.flowId;

    acs.duplicatePktsAtDropInstance =
        flowIdToPennyFlowMap[acs.flowId].getDuplicatesByPacketDropId(acs.packetId);

    for (const auto& x : flowIdToPennyFlowMap)
    {
        struct statsSnapshot cs = flowIdToPennyFlowMap[x.first].getCurFlowState();
        acs.counters.totalPkts += cs.counters.totalPkts;
        acs.counters.dataPkts += cs.counters.dataPkts;
        acs.counters.pureAckPkts += cs.counters.pureAckPkts;
        acs.counters.droppablePkts += cs.counters.droppablePkts;
        acs.counters.inOrderPkts += cs.counters.inOrderPkts;
        acs.counters.outOfOrderPkts += cs.counters.outOfOrderPkts;
        acs.counters.droppedPkts += cs.counters.droppedPkts;
        acs.counters.retransmittedDroppedPkts += cs.counters.retransmittedDroppedPkts;
        acs.counters.notSeenDroppedPkts += cs.counters.notSeenDroppedPkts;
        acs.counters.duplicatePkts += cs.counters.duplicatePkts;
        acs.counters.pendingDroppedPkts += cs.counters.pendingDroppedPkts;

        acs.lists[x.first].droppedPcksList = cs.lists.droppedPcksList;
        acs.lists[x.first].expiredPcksList = cs.lists.expiredPcksList;
        acs.lists[x.first].retransmittedPktsList = cs.lists.retransmittedPktsList;

        acs.flowsContributed++;
    }
    pendingDropSnapsList.push_back(acs);
}

json penny::exportFlowCountersJson(struct pennyCounters counters)
{
    json exportData;

    exportData["totalPkts"] = counters.totalPkts;
    exportData["dataPkts"] = counters.dataPkts;
    exportData["pureAckPkts"] = counters.pureAckPkts;
    exportData["droppablePkts"] = counters.droppablePkts;
    exportData["inOrderPkts"] = counters.inOrderPkts;
    exportData["outOfOrderPkts"] = counters.outOfOrderPkts;
    exportData["droppedPkts"] = counters.droppedPkts;
    exportData["retransmittedDroppedPkts"] = counters.retransmittedDroppedPkts;
    exportData["notSeenDroppedPkts"] = counters.notSeenDroppedPkts;
    exportData["duplicatePkts"] = counters.duplicatePkts;
    exportData["pendingDroppedPkts"] = counters.pendingDroppedPkts;

    return exportData;
}

json penny::exportToJson(bool indivFlowsStats)
{
    json exportData;

    /* Export aggregates */
    exportData["aggregates"]["aggrOutcome"] = aggrOutcome;
    exportData["aggregates"]["finalOutcome"] = finalOutcome;

    for (auto it = indivFlowsClosedLoop.begin(); it != indivFlowsClosedLoop.end(); ++it)
    {
        exportData["aggregates"]["indivFlowsClosedLoop"] += *it;
    }

    /* Iterate over the snapshots */
    int index = 0;
    for (std::list<struct aggrCounterSnapshot>::iterator iter = evaluatedSnapsList.begin(); iter != evaluatedSnapsList.end(); ++iter)
    {
        exportData["snapshots"][index]["counters"] = exportFlowCountersJson(iter->counters);

        exportData["snapshots"][index]["droppedPcksList"] = std::list<std::string>();
        exportData["snapshots"][index]["expiredPcksList"] = std::list<std::string>();
        exportData["snapshots"][index]["retransmittedPktsList"] = std::list<std::string>();

        for (const auto& x : iter->lists)
        {
            for (const auto& dropped : x.second.droppedPcksList)
            {
                exportData["snapshots"][index]["droppedPcksList"].push_back("(" + x.first + "," +
                                                                            dropped + ")");
            }
            for (const auto& expired : x.second.expiredPcksList)
            {
                exportData["snapshots"][index]["expiredPcksList"].push_back("(" + x.first + "," +
                                                                            expired + ")");
            }
            for (const auto& retransmitted : x.second.retransmittedPktsList)
            {
                exportData["snapshots"][index]["retransmittedPktsList"].push_back(
                    "(" + x.first + "," + retransmitted + ")");
            }
        }

        exportData["snapshots"][index]["flowId"] = iter->flowId;
        exportData["snapshots"][index]["packetId"] = iter->packetId;
        index++;
    }
    if (indivFlowsStats)
    {
        for (auto x = flowIdToPennyFlowMap.cbegin(), next_it = x; x != flowIdToPennyFlowMap.cend();
             x = next_it)
        {
            ++next_it;
            exportData["indivFlows"][x->first] =
                flowIdToPennyFlowMap[x->first].exportFlowStatsJson();
        }
    }
    return exportData;
}