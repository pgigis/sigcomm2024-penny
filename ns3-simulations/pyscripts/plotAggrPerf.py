import os
import json
import argparse
import numpy as np
import statistics

from scipy import stats
from matplotlib import pyplot as plt

plt.rcParams['pdf.fonttype'] = 42
plt.rcParams['ps.fonttype'] = 42

def ecdf(sample):
    """
    Compute the empirical cumulative distribution function (ECDF) for a sample of data.

    Parameters:
    sample (array-like): Sample data.

    Returns:
    tuple: Sorted sample data and cumulative probabilities.
    """
    sample = np.atleast_1d(sample)
    quantiles, counts = np.unique(sample, return_counts=True)
    cumprob = np.cumsum(counts).astype(np.double) / sample.size
    # Ensure the ECDF starts from (0, 0)
    quantiles = np.insert(quantiles, 0, quantiles[0])
    cumprob = np.insert(cumprob, 0, 0)
    return quantiles, cumprob

def plot_figure(data, filename):
    """
    Plot the empirical cumulative distribution functions (ECDFs) for different data sets and save the plot.

    Parameters:
    data (dict): Dictionary containing the data to plot.
    filename (str): The filename to save the plot.
    """
    plotData_x = {}
    plotData_y = {}

    for key in data:
        data[key].sort()
        plotData_x[key], plotData_y[key] = ecdf(data[key])

    fig, ax = plt.subplots(figsize=(5,4))

    ax.plot(plotData_x['baseline'], plotData_y['baseline'], color="blue", lw=1, label='Penny disabled')
    ax.plot(plotData_x['penny5'], plotData_y['penny5'], color="red", lw=1, label='Penny enabled')

    ax.legend(fancybox=True, loc='lower right' , fontsize=10)
    ax.set_xlabel('Flow completion time (in secs)', fontsize=14)
    ax.set_ylabel('Cumulative probability', fontsize=14)
    plt.yticks(np.arange(0, 1.1, 0.1))
    plt.xticks(np.arange(2, 18, 2),rotation=0)
    plt.grid(True, linestyle='--', alpha=0.25)
    fig.tight_layout()

    # Save the plot
    plt.savefig(filename, format='png', dpi=900)
    plt.close(fig)

def extract_data(folder_name, packet_number):
    """
    Extract data from JSON files in the specified folder for a specific packet number.

    Parameters:
    folder_name (str): The folder containing the input files.
    packet_number (int): The packet number to extract data for.

    Returns:
    dict: Dictionary containing extracted data categorized by different loss types.
    """
    packet_number = str((packet_number * 1024) + 2)
    
    data = {
        'baseline': [],
        'penny5': []
    }

    for filename in os.listdir(folder_name):
        if 'perf_' not in filename:
            continue

        if 'perf_type2noLoss_0.000000' in filename:
            pointer = 'baseline'
        elif 'perf_type2noLoss_0.050000' in filename:
            pointer = 'penny5'
        else:
            print(f"Error: Filename '{filename}' not recognized")
            continue

        file_path = os.path.join(folder_name, filename)
        if os.path.isfile(file_path):
            try:
                with open(file_path) as f:
                    for line in f:
                        json_data = json.loads(line.rstrip().split('\t')[1])
                        time = float(json_data['acks'][packet_number]) - float(json_data['s'])
                        data[pointer].append(time)
            except Exception as e:
                print(f"Couldn't process '{file_path}': {e}")
    return data

def main():
    """
    Main function to parse arguments and generate the accuracy figure.
    """
    parser = argparse.ArgumentParser(description="Generate accuracy figure.")
    parser.add_argument('-f', dest='folder_name', help="The folder to take as input.", type=str, required=True)
    parser.add_argument('-o', dest='output_file', help="The filename output.", type=str, required=True)
    parser.add_argument('-p', dest='packet_number', help="The packet number to print.", type=int, required=True)
    args = parser.parse_args()

    data = extract_data(args.folder_name, args.packet_number)
    plot_figure(data, args.output_file)

if __name__ == '__main__':
    main()
