# Set terminal to PDF and specify output file
set terminal pdfcairo enhanced color solid size 8in, 6in
set output 'closed-loop-theoretical-5perc.pdf'

# Configure multiplot layout
set multiplot

# First plot: main plot
set xlabel "# of droppable packets" font "Helvetica,24" offset 0,-1
set ylabel "P(genuine)" font "Helvetica,24" offset -4,0
set tics font "Helvetica,18"
set xtics
set ytics
set xrange [0:300]
set lmargin 18
set rmargin 4
set bmargin 5
set tmargin 2
plot for [i=2:45] 'tempFiles/d'.i using 2:10 w l notitle

# Second plot: inset
set size 0.6, 0.5       # set size of inset
set origin 0.37, 0.45     # move bottom left corner of inset
set xlabel "" font "Helvetica,24" offset 0,-1
set ylabel "" font "Helvetica,24" offset -6,0
set tics font "Helvetica,18"
set xtics
set ytics ("2.5x10⁻⁴" 2.5e-4, "2x10⁻⁴" 2e-4, "1.5x10⁻⁴" 1.5e-4, "1x10⁻⁴" 1e-4, "0.5x10⁻⁴" 0.5e-4, "1x10⁻⁵" 1e-5)
set xrange [200:300]
set lmargin 18
set rmargin 4
set bmargin 5
set tmargin 2
plot for [i=2:45] 'tempFiles/d'.i using 2:10 w l notitle

# Reset multiplot configuration
unset multiplot

# Reset output terminal
set output
