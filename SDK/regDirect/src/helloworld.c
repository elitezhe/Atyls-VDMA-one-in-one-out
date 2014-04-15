/*
 * Copyright (c) 2009-2012 Xilinx, Inc.  All rights reserved.
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xiic.h"
#include "xparameters.h"
#include "xil_io.h"
#include "xintc.h"
#include "edid.h"
#include "xgpio.h"
#include "user.h"

#define    INTC_DEVICE_ID        XPAR_MICROBLAZE_0_INTC_DEVICE_ID
#define    BUTTON_DEVICE_ID      XPAR_PUSH_BUTTONS_5BITS_DEVICE_ID
#define    LED_DEVICE_ID         XPAR_LEDS_8BITS_DEVICE_ID

//vdma macro
#define    VDMA_BASE_ADDR        XPAR_AXIVDMA_0_BASEADDR

XIntc intCtrl;
XGpio pushButton;
XGpio led;
//XAxiVdma vdma;

int Status;


void print(char *str);
inline void writeVdma(u32 offset, u32 value);

int main()
{
    init_platform();

    print("Hello World\r\n");
    XIntc_Initialize(&intCtrl, INTC_DEVICE_ID);

	EdidInit(&intCtrl);

	microblaze_register_handler(XIntc_DeviceInterruptHandler, INTC_DEVICE_ID);
	microblaze_enable_interrupts();
	XIntc_Start(&intCtrl, XIN_REAL_MODE);

	XGpio_Initialize(&pushButton, BUTTON_DEVICE_ID);
	XGpio_SetDataDirection(&pushButton, 1, 0xff);

	XGpio_Initialize(&led, LED_DEVICE_ID);
	XGpio_SetDataDirection(&led, 1, 0x00);
	//test led
	XGpio_DiscreteSet(&led, 1, 0xff);
	wait(1000);
	XGpio_DiscreteSet(&led, 1, 0x00);

	//wait edid read finish
	int edidRdFinish = 0;
	u32 buttonValue;
	//iicIntTimes = 0;
	while(edidRdFinish == 0)// not finished
	{
		XGpio_DiscreteSet(&led, 1, iicIntTimes);
		buttonValue = XGpio_DiscreteRead(&pushButton, 1);
		if((buttonValue & 0x01) != 0 ) //centre button
		{
			edidRdFinish = 1; //exit loop, go to vdma setup
			XGpio_DiscreteSet(&led, 1, 0xff);
			wait(1000);
			XGpio_DiscreteSet(&led, 1, 0x00);
		}

	}
	Xil_Out32(XPAR_AXI_HDMI_0_BASEADDR, 1280*720);
	u32 ResolutionValue;
	ResolutionValue = Xil_In32(XPAR_AXI_HDMI_0_BASEADDR);
	//putnum(ResolutionValue);
	xil_printf("HDMI Base Value is: 0x%x \r\n", ResolutionValue);

	writeVdma(0x00, 0x0001008b);
	//writeVdma(0x, 0x);
	writeVdma(0x30, 0x00010043);
	writeVdma(0x5c, 0xa9000000);
	writeVdma(0x60, 0xaa000000);
	writeVdma(0x64, 0xab000000);

	writeVdma(0xac, 0xa9000000);
	writeVdma(0xb0, 0xaa000000);
	writeVdma(0xb4, 0xab000000);

	writeVdma(0x58, 0x00000780);
	writeVdma(0x54, 0x00000780);

	writeVdma(0xa8, 0x00000780);
	writeVdma(0xa4, 0x00000780);

	writeVdma(0xa0, 0x000001e0);
	writeVdma(0x50, 0x000001e0);

	while(1)
	{
		wait(2000);
		u32 value;
		value = Xil_In32(VDMA_BASE_ADDR);
		printf("offset 0x00 is: 0x%x \r\n",value);
		value = Xil_In32(VDMA_BASE_ADDR+0x04);
		printf("offset 0x04 is: 0x%x \r\n",value);
		value = Xil_In32(VDMA_BASE_ADDR+0x30);
		printf("offset 0x30 is: 0x%x \r\n",value);
		value = Xil_In32(VDMA_BASE_ADDR+0x34);
		printf("offset 0x34 is: 0x%x \r\n",value);

	}

    return 0;
}


void writeVdma(u32 offset, u32 value)
{
	Xil_Out32((VDMA_BASE_ADDR+offset), value);
}
