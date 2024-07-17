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

mkdir -p ../tempResults/accuracyMixedEqual/

for seed in $(seq 1 "$execution_runs"); do
    for topologyType in type1_noLoss.json type1_LossBoth1.json type1_LossBoth3.json type1_LossBoth6.json \
                        type1_LossUpstream1.json type1_LossUpstream3.json type1_LossDownstream6.json \
                        type1_LossDownstream1.json type1_LossDownstream3.json type1_LossUpstream6.json; do

        for pennyConf in drop1_min300pkts.json drop2_min300pkts.json drop3_min300pkts.json drop4_min300pkts.json drop5_min300pkts.json; do

            check_process_instances 
            {
                ./ns3 run --no-build "scratch/penny/sim.cc --argSeed=$seed --argExperimentConf=accuracyMixedEqual.json --argTopologyConf=$topologyType --argPennyConf=$pennyConf"
            } &

            sleep 0.01  # Brief pause to manage load

        done
    done
done

mkdir -p ../tempResults/accuracyMixedEqualWithDup/


for seed in $(seq 1 "$execution_runs"); do
    for topologyType in type1_noLoss.json type1_LossBoth1.json type1_LossBoth3.json type1_LossBoth6.json \
                        type1_LossUpstream1.json type1_LossUpstream3.json type1_LossDownstream6.json \
                        type1_LossDownstream1.json type1_LossDownstream3.json type1_LossUpstream6.json; do

        for pennyConf in drop1_min300pkts.json drop2_min300pkts.json drop3_min300pkts.json drop4_min300pkts.json drop5_min300pkts.json; do

            check_process_instances
            {
                ./ns3 run --no-build "scratch/penny/sim.cc --argSeed=$seed --argExperimentConf=accuracyMixedEqualWithDup.json --argTopologyConf=$topologyType --argPennyConf=$pennyConf"
            } &

            sleep 0.01  # Brief pause to manage load

        done
    done
done