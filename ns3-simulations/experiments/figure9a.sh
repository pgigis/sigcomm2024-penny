#!/bin/bash

# Check if sufficient arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <max_parallel_instances> <execution_runs>"
    exit 1
fi

# Read arguments
max_parallel_instances=$1
execution_runs=$2

# Build new changes
./ns3

# Function to check the number of running instances of the process
check_process_instances() {
    local count=$(pgrep -c "ns3")
    echo "Number of ns3 instances running: $count"
    while [ "$count" -gt "$max_parallel_instances" ]; do
        echo "Exceeded the maximum number of instances ($max_parallel_instances). Waiting for 4 seconds..."
        sleep 4
        count=$(pgrep -c "ns3")
        echo "Number of ns3 instances running: $count"
    done
}

mkdir -p tempResults/aggregatesFlowPerformance/

# Run baseline
for seed in $(seq 1 "$execution_runs"); do
    check_process_instances
    {
        ./ns3 run --no-build "scratch/penny/sim.cc --argSeed=$seed --argExperimentConf=aggregatesPerformancePennyDisabled.json --argTopologyConf=type2_noLoss.json --argPennyConf=disabled.json"
        python3 pyscripts/extracIndivPerf.py -f "tempResults/aggregatesFlowPerformance/type2noLoss_0.000000_${seed}-traces.txt" > "tempResults/aggregatesFlowPerformance/perf_type2noLoss_0.000000_${seed}.txt"
        rm "tempResults/aggregatesFlowPerformance/type2noLoss_0.000000_${seed}-traces.txt"
    } &

    sleep 0.01  # Brief pause to manage load
done

# Run Penny with 5% drop and 12 drops at amx
for seed in $(seq 1 "$execution_runs"); do
    check_process_instances
    {
        ./ns3 run --no-build "scratch/penny/sim.cc --argSeed=$seed --argExperimentConf=aggregatesPerformancePennyEnabled.json --argTopologyConf=type2_noLoss.json --argPennyConf=drop5_12pkts_aggrMin100.json"
        python3 pyscripts/extracIndivPerf.py -f "tempResults/aggregatesFlowPerformance/type2noLoss_0.050000_${seed}-traces.txt" > "tempResults/aggregatesFlowPerformance/perf_type2noLoss_0.050000_${seed}.txt"
        rm "tempResults/aggregatesFlowPerformance/type2noLoss_0.050000_${seed}-traces.txt"
    } &

    sleep 0.01  # Brief pause to manage load
done