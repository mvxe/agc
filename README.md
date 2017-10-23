# Alpha Gamma Counter

This is a program for the Red Pitaya board that turns it into a event counter able to do correlation counting. 
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

