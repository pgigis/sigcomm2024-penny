import os
import json
import argparse
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# Define color mapping for different statuses
color_mapping = {0: 'darkorange', 1: 'lightgrey', 2: 'darkblue'}

# Create custom legend handles
legend_handles = [
    Patch(color='darkorange', label='stats => closed-loop'),
    Patch(color='darkblue', label='stats => spoofed'),
    Patch(color='lightgrey', label='duplicates exceeded'),
    Patch(color='seagreen', label='Penny\'s threshold')
]

def evaluate_hypotheses(data):
    """
    Evaluate packet data to determine the hypothesis based on provided counters.
    """
    # Calculate the denominator for duplicate factor calculation
    fdupDenominator = data['droppablePkts'] - data['droppedPkts']

    # If denominator is less than or equal to zero, no decision can be made
    if fdupDenominator <= 0:
        return "no-decision"

    # Calculate the duplicate factor
    f_dup = data['duplicatePkts'] / fdupDenominator if data['duplicatePkts'] != 0 else 1.0 / fdupDenominator

    # Check if duplicates exceed the threshold
    if f_dup > 0.15:
        return "duplicateExceeded"

    # Calculate hypotheses probabilities
    hypothesisH1 = pow(0.05, data['notSeenDroppedPkts'])
    hypothesisH2 = pow(f_dup, data['retransmittedDroppedPkts'])
    probability_bidirectional = hypothesisH1 / (hypothesisH1 + hypothesisH2)

    # Determine the hypothesis based on probability
    if probability_bidirectional > 0.99:
        return "closed-loop"
    elif probability_bidirectional < 0.01:
        return "spoofed"
    else:
        return "no-decision"

def extract_data(folder_name):
    """
    Extract data from JSON files in the specified folder.
    """
    data = {'x': [], 'y': [], 'z': []}

    # Iterate through each file in the folder
    for filename in os.listdir(folder_name):
        file_path = os.path.join(folder_name, filename)
        if os.path.isfile(file_path):
            try:
                # Open and read JSON data
                with open(file_path, 'r') as f:
                    data_json = json.load(f)
                    if 'snapshots' in data_json:
                        for drop in data_json['snapshots']:
                            eval_result = evaluate_hypotheses(drop['counters'])
                            droppable_pkts = int(drop['counters']['droppablePkts'])
                            dropped_pkts = int(drop['counters']['droppedPkts'])
                            
                            # Append data based on evaluation result
                            if eval_result == 'spoofed':
                                data['x'].append(droppable_pkts)
                                data['z'].append(2)
                                data['y'].append(dropped_pkts - 0.50)
                            elif eval_result == 'closed-loop':
                                data['x'].append(droppable_pkts)
                                data['z'].append(0)
                                data['y'].append(dropped_pkts - 0.25)
                            elif eval_result == 'duplicateExceeded':
                                data['x'].append(droppable_pkts)
                                data['z'].append(1)
                                data['y'].append(dropped_pkts - 0.75)
            except Exception as e:
                print(f"Couldn't process '{file_path}': {e}")
    return data

def plot_figure(data, filename):
    """
    Plot the extracted data and save the figure to a file.
    """
    fig, ax = plt.subplots()

    # Configure plot axes and grid
    ax.set_ylim(0, 25)
    ax.set_xlim(0, 600)
    ax.set_yticks(np.arange(0, 25, 2))
    ax.set_xticks(np.arange(0, 700, 100))
    ax.grid(color='lightgrey', linestyle='-', linewidth=0.25)
    ax.hlines(y=12, xmin=0, xmax=1000, linewidth=1.8, color='seagreen', alpha=0.9)
    ax.hlines(y=11.85, xmin=0, xmax=1000, linewidth=1.5, color='seagreen', alpha=0.9)
    ax.hlines(y=12.15, xmin=0, xmax=1000, linewidth=1.5, color='seagreen', alpha=0.9)
    
    # Ensure that the grid is drawn below the plotted data
    ax.set_xlim(left=0)

    # Create the scatter plot
    point_colors = [color_mapping[color] for color in data['z']]
    ax.scatter(data['x'], data['y'], c=point_colors, s=1)
    ax.set_xlabel('# of droppable packets', fontsize=14)
    ax.set_ylabel('# of packet drops', fontsize=14)

    # Customize ticks and legend
    plt.xticks(fontsize=10)
    plt.yticks(fontsize=10)
    #plt.legend(handles=legend_handles, loc='upper right')

    # Adjust layout and save the plot
    plt.tight_layout()
    plt.savefig(filename, format='png', dpi=900)
    plt.close(fig)

def main():
    """
    Main function to parse arguments and generate the accuracy figure.
    """
    parser = argparse.ArgumentParser(description="Generate accuracy figure.")
    parser.add_argument('-f', dest='folder_name', help="The folder to take as input.", type=str, required=True)
    parser.add_argument('-o', dest='output_file', help="The filename output.", type=str, required=True)
    args = parser.parse_args()

    data = []
    if ',' in args.folder_name:
        filesToRead = args.folder_name.split(',')
        for file in filesToRead:
            data = data + extract_data(args.folder_name)
    else:
        data = extract_data(args.folder_name)

    plot_figure(data, args.output_file)

if __name__ == '__main__':
    main()
