# Alpha Gamma Counter

This program turns the Red Pitaya board into an event counter able to do correlation counting. 
Originally made for Alpha - Gamma correlation counting on an Am-241 source, it can be modified and adjusted for different systems as well.

## FPGA
The FPGA folder contains files needed to compile the .bit file for the FPGA. The initial code was taken from 
<https://github.com/RedPitaya/RedPitaya/tree/master/fpga> (an older version, check the commit dates as the history was preserved).
**See file FPGA/rtl/ssla.v for the counter code.**
To compile the FPGA code you need vivado 2015.4 (or modify Makefile and possibly other files to support a newer version).
To compile (replace PATH with your vivado installation folder) run:
```
cd FPGA
source PATH/Vivado/2015.4/settings64.sh
make FPGA_TOOL=vivado clean
make FPGA_TOOL=vivado out/red_pitaya.bit
```
The FPGA-binaries folder contains precompiled binaries.

## RPTY
This program runs on the Red Pitaya CPU. You should copy the RPTY folder over to the Red Pitaya.
You also need to copy the red_pitaya_agc_vX.X.bit binary to the same folder. Upon execution, the program automatically loads the binary file into the FPGA.
You can compile it there, but you need a Red Pitaya image with gcc (not all of them have gcc installed).
Compile with:
```
cmake .
make
```
Run with:
```
./agc
```
The program is interactive, so just follow instructions.
In the end the program outputs four files:
```
	alpha.dat
	gamma.dat
	time.dat
	timesum.dat
```	
They are all in binary format '%uint32' as explained by the program. 
They may be plotted with gnuplot:
```
plot "alpha.dat" binary format='%uint32' using ($0):1 with lines notitle
plot "gamma.dat" binary format='%uint32' using ($0):1 with lines notitle
plot "timesum.dat" binary format='%uint32' using ($0/125000000):1 with lines notitle
```
The file time.dat is a 3D data array and cannot be plotted easily.
To process time.dat see <https://github.com/mvxe/agc-proc>
