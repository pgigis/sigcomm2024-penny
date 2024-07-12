# Penny

Welcome to the repository for the artifacts of the paper "Bad Packets Come Back, Worse Ones Don't".

All artifacts will be available soon. Stay tuned for updates!

Table of Contents
* [Project Website](#project-website)
* [Getting Started](#getting-started)
    * [Prerequisites and Installation](#prerequisites-and-installation)

## Project Website 
Explore our findings through our project website: (Coming Soon..)

## Getting Started
### Prerequisites and Installation
The software is written in C++ and Python3. We provide all the necessary libraries, which need to be pre-installed on your system.


## Artifacts Overview
The artifacts are divided into two main sections: theoretical analysis and NS-3 simulations.

The [Theoretical analysis](#theoretical-analysis) section provides the necessary tools to generate the results discussed in Section 5.1, particularly  generating Figure 7.

The [NS-3 simulations](#ns-3-simulation) section provides the necessary tools to reproduce the results presented in Section 6 (Evaluation), specifically for generating Figures 8 and 9.

## Theoretical analysis
For the theoretical analysis, we provide a single bash script that executes the simulator and generates Figure 7. Please note that the simulator's execution time can vary significantly, ranging from 30 minutes to a few hours. Additionally, due to the randomness in the simulation process, the generated figure might exhibit minor differences based on the seed and random function implementation.

To ensure exact reproducibility, we have included the files generated by the simulator during our run. By using these files, you can reproduce the exact figure presented in the paper. These files are located in the `theoretical-analysis/paper-results/tempFiles` directory.

To reproduce the theoretical analysis, please follow these steps:

Navigate to the 'theoretical-analysis' directory:
```
cd theoretical-analysis
```

Execute the simulation script:
```
bash run.sh
```

After execution, the 'results' folder will contain the following two files:
 - `closed-loop-theoretical-5perc.pdf`: Generated from the output of the simulator you just executed.
 - `closed-loop-theoretical-5perc-paper.pdf`: Generated from the provided files from our simulation execution.

