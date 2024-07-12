#!/bin/bash

# Check if tempFiles/results.txt exists
if [ -f tempFiles/results.txt ]; then
  echo "File 'tempFiles/results.txt' already exists. Skipping simulator execution."
else
  # Execute simulator if tempFiles/results.txt does not exist
  echo "Compiling the simulator..."
  g++ -O2 -o simulator sim.cc -lm

  echo "Creating tempFiles directory..."
  mkdir -p tempFiles

  echo "Running the simulator..."
  ./simulator > tempFiles/results.txt
fi

# Continue with data extraction and plotting
echo "Extracting data from results.txt..."
for i in {2..45}; do
  grep "d $i " tempFiles/results.txt > tempFiles/"d$i"
done

echo "Generating plots with gnuplot..."
gnuplot -persist plot.gp

echo "Generating [paper/submitted version] plots with gnuplot..."
gnuplot -persist paper-results/plot.gp

#echo "Cleaning up temporary files..."
#rm -rf tempFiles

echo "Creating results directory..."
mkdir -p ../results
	
echo "Moving generated pdfs..."
mv *.pdf ../results

echo "Script execution completed."
