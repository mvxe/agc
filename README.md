# Alpha Gamma Counter

This program turns the Red Pitaya board into an event counter able to do correlation counting. 
Originally made for Alpha - Gamma correlation counting on an Am-241 source, it can be modified and adjusted for different systems as well.

##FPGA
The FPGA folder contains files needed to compile the .bit file for the FPGA. The initial code was taken from 
<https://github.com/RedPitaya/RedPitaya/tree/master/fpga> (an older version, check the commit dates as the history was preserved).
To compile the FPGA code you need vivado 2015.4 (or modify Makefile and possibly other files to support a newer version).
To compile (replace PATH with your vivado installation folder) run:
%	cd FPGA
%	source PATH/Vivado/2015.4/settings64.sh
%	make FPGA_TOOL=vivado clean
%	make FPGA_TOOL=vivado out/red_pitaya.bit
The FPGA-binaries folder contains precompiled binaries.

##RPTY
This program runs on the Red Pitaya CPU. You should copy the RPTY folder over to the Red Pitaya.
You also need to copy the red_pitaya_agc_vX.X.bit binary to the same folder. Upon execution, the program automatically loads the binary file into the FPGA.
You can compile it there, but you need a Red Pitaya image with gcc (not all of them have gcc installed).
Compile with:
%	cmake .
%	make
Run with:
%	./agc
The program is interactive, so just follow instructions.
