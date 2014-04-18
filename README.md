Atyls-VDMA-one-in-one-out
=========================

Board: Digilent Atyls
OS: Win7 x64
ISE: 14.7 nt64

*IP cores:
	*VDMA
	*HDMI(supply by Digilent, can be found on Atyls BSB support package)
	*GPIO
	
*Make this system to work:
	*1.Generate netlist, bitstream and export to SDK;
	*2.Create a SDK project, using standlone C;
	*3.Add all src files in regDirect file clip into your project;
	*4.Download FPGA program files into FPGA by jtag, download elf files to FPGA;
	*5.Connect J3 to Kindle Fire HD, wait onboard led stop blink, press centre button;
	*6.Connect J2 to your monitor, video from kindle should have display on your monitor.