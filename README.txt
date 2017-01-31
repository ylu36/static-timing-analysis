node.cpp contains the parser for phase 1 and the calculation functions for phase 2. 
Use "make" command to compile this program. Use "make clean" to remove all executables
The executable is called "parser". 
To run phase 1 code, use "./parser read_ckt [bench_file's_name]". A ckt_details.txt will be produced with the expected output.
use "./parser read_nldm delays sample_NLDM.lib.txt" to produce a "delay_LUT.txt" which contains all the delay information.
use "./parser read_nldm slews sample_NLDM.lib.txt" to produce a "slew_LUT.txt" which contains all the slew information.

