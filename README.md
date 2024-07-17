# _"Bad Packets Come Back, Worse Ones Don't"_

**Table of Contents:**
* [Prerequisites and Installation Instructions](#prerequisites-and-installation-instructions)
* [Artifacts Overview](#artifacts-overview)
    * [Theoretical Analysis](#theoretical-analysis)
    * [NS-3 Simulations](#ns-3-simulations)
        * [Reproducing Accuracy Results](#reproducing-accuracy-results)
        * [Reproducing Perfomance Results](#reproducing-performance-results)

## Prerequisites and Installation Instructions
The artifacts software is developed using C++ and Python3. Before proceeding with the installation, please ensure the following dependencies are installed on your system:

#### Step 1: Install Basic Packages
Open a terminal and execute the following commands to install `cmake`, `g++`,  `python3`, and `gnuplot`:
 
For Debian/Ubuntu-based distributions execute the following commands:
```bash
sudo apt update
sudo apt install cmake g++ python3 gnuplot
```

##### Install Python Dependencies
To install the required Python dependencies, we recommend setting up a virtual environment. After that, run:
```bash
pip3 install -r requirements.txt
```

If you encounter any issues with the installation or find that a required dependency is missing, please contact at p.gkigkis (at) cs.ucl.ac.uk for assistance.


#### Step 2: Install the custom NS-3 Simulator

Before installing the ns-3 simulator, please ensure that you have installed the necessary dependencies as outlined in the previous steps. It is essential to use the provided version of the ns-3 simulator (version 3.40) because we have made modifications to the simulator core to enable packet drops and random link loss.

To install the simulator navigate to `ns3-simulations`:
```bash
cd ns3-simulations
```

Run the configuration script to set up the simulator environment:
```bash
./ns3 configure
```

Finally, build the ns-3 simulator by executing the following command:
```bash
./ns3 build
```

After following these steps, the ns-3 simulator should be installed and ready to use. 
If you encounter any issues during installation, please contact us.


## Artifacts Overview

The artifacts are divided into two main sections: theoretical analysis and NS-3 simulations.

The [Theoretical Analysis](#theoretical-analysis) section provides the necessary tools to generate the results discussed in Section 5.1, particularly  generating Figure 7.

The [NS-3 Simulations](#ns-3-simulations) section provides the necessary tools to reproduce the results presented in Section 6 (Evaluation), specifically for generating Figures 8 and 9.

### Theoretical Analysis
For the theoretical analysis, we have developed a single bash script that executes the simulator and generates Figure 7 from our paper. Please note that the simulator's execution time can vary significantly, ranging from 30 minutes to a few hours. Additionally, due to the randomness in the simulation process, the generated figure might exhibit minor differences based on the seed and random function implementation.

To ensure exact reproducibility, we have included the files generated by the simulator during our run. By using these files, you can reproduce the exact figure presented in the paper. These files are located in the `theoretical/paper-results/tempFiles` directory.

To reproduce the theoretical analysis, please follow these steps:

Navigate to the `theoretical-analysis` folder:
```bash
cd theoretical-analysis
```

Execute the simulation script:
```bash
bash run.sh
```

Upon successful execution of the script, you will find the following two plots in the `results` folder:
 - `closed-loop-theoretical-5perc.pdf`: Generated from the output of the simulator you just executed.
 - `closed-loop-theoretical-5perc-paper.pdf`: Generated from the provided files from our simulation execution.


### NS-3 Simulations
For the evaluation, we used NS-3 simulator (version 3.40). As part of the artifacts we provide a lightweight implementation of Penny in C++. 

You can find the source code for the lightweight implementation of Penny in the following directory:
`ns3-simulations/scratch/penny`.

We provide an easy way to configure the Penny, NS-3 topology, and simulation parameters using `JSON`-formatted files. 
The configuration files can be found in folder `ns3-simulations/scratch/penny/configs`.

In the provided simulations, we use a dumbbell topology with two routers connected by a bottleneck link.

### Reproducing Accuracy Results
In this section, we provide detailed instructions on how to generate Figure 8 from the paper.

To do this, first navigate to the `ns3-simulations` folder:
```bash
cd ns3-simulations
```

**Notes:**

 - For the simulations, we recommend conducting at least 500 experiments. 

 - Due to the randomness in the simulation process and the number of executions, the generated figures may exhibit minor differences.
#### Step 1: Run the Experiments

Execute the following command to start the simulations for Figure 8. Replace `<number_of_parallel_runs>` and `<number_of_experiments>` with the appropriate values for your setup: 

####  Figure 8a - Only closed-loop flows
```bash
bash experiments/figure8a.sh <number_of_parallel_runs> <number_of_experiments> 
```
#### Figure 8b - Mixed 50/50 closed-loop/not closed loop flows
```bash
bash experiments/figure8b.sh <number_of_parallel_runs> <number_of_experiments> 
```

#### Figure 8c - Only not closed-loop
```bash
bash experiments/figure8c.sh <number_of_parallel_runs> <number_of_experiments> 
```

#### Step 2: Plot the Results
To generate the plot for Figure 8a execute the following command:

#### Figure 8a - Only closed-loop flows
```bash
python3 pyscripts/plotAccuracy.py -f tempResults/accuracyOnlyClosedLoop -o plots/accuracyOnlyClosedLoop.png
```
#### Figure 8b - Mixed 50/50 closed-loop/not closed loop flows
```bash
python3 pyscripts/plotAccuracy.py -f tempResults/accuracyMixedEqual,tempResults/accuracyMixedEqualWithDup -o plots/accuracyMixedClosedNotClosedLoops.png
```
#### Figure 8c - Only not closed-loop
```bash
python3 pyscripts/plotAccuracy.py -f tempResults/accuracyOnlyNotClosedLoop -o plots/accuracyOnlyNotClosedLoop.png
```

**Note:** For Figure 8b, you can also generate figures for the two individual scenarios:
1.  The not closed-loop flows do not send any duplicates.
```bash
python3 pyscripts/plotAccuracy.py -f tempResults/accuracyMixedEqual -o plots/accuracyMixedEqual.png
```
2.  The not closed-loop flows send duplicates at a rate of 14.9%.
```bash
python3 pyscripts/plotAccuracy.py -f tempResults/accuracyMixedEqualWithDup -o plots/accuracyMixedEqualWithDup.png
```

### Reproducing Performance Results
