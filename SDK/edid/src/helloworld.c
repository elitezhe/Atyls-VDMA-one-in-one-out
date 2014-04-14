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
#include "xparameters.h"
#include "xintc.h"
#include "xgpio.h"
#include "xil_io.h"
#include "mb_interface.h"
#include "xiic.h"

//常量
#define    INTC_DEVICE_ID        XPAR_MICROBLAZE_0_INTC_DEVICE_ID
#define    IIC_IRPT_ID           XPAR_MICROBLAZE_0_INTC_AXI_IIC_0_IIC2INTC_IRPT_INTR

#define    IIC_BASEADDR          XPAR_IIC_0_BASEADDR
#define    IIC_DEVICE_ID         XPAR_AXI_IIC_0_DEVICE_ID

/*
 * IIC bit definitions
 */
#define bitAasFlag 0x00000020
#define bitTxEmptyFlag 0x00000004
#define bitTxDoneFlag 0x00000002
/*
 * IIC Register offsets
 */
#define bIicGIE 0x001C //Global Int Enable
#define bIicISR 0x0020 //Int status reg
#define bIicIER 0x0028 // int enable reg
#define bIicCR  0x0100 //control reg
#define bIicSR  0x0104 //status reg
#define bIicTX  0x0108 //Transmit FIFO
#define bIicRX  0x010C //Receive FIFO
#define bIicADR 0x0110 //Slave Addr
#define bIicTXOCY 0x0114 //Transmit FIFO Occupancy Reg
/*
 * EDID array definition. Changing these values changes the EDID
 * packet that is sent over the E-DDC wires. It contains information on which
 * resolutions the device supports. Currently, this packet provides fairly
 * generic resolution support. Note that using resolutions with widths larger
 * than the line stride of the hdmi_output core results in a choppy picture.
 */
static const u8 rgbEdid[256] =
{0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x11,0x27,0x00,0x00,0x00,0x00,0x00,0x00,
 0x2C,0x15,0x01,0x03,0x80,0x26,0x1E,0x78,0x2A,0xE5,0xC5,0xA4,0x56,0x4A,0x9C,0x23,
 0x12,0x50,0x54,0xBF,0xEF,0x80,0x8B,0xC0,0x95,0x00,0x95,0x0F,0x81,0x80,0x81,0x40,
 0x81,0xC0,0x71,0x4F,0x61,0x4F,0x6B,0x35,0xA0,0xF0,0x51,0x84,0x2A,0x30,0x60,0x98,
 0x36,0x00,0x78,0x2D,0x11,0x00,0x00,0x1C,0x00,0x00,0x00,0xFD,0x00,0x38,0x4B,0x1F,
 0x50,0x0E,0x0A,0x0A,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFC,0x00,0x41,
 0x54,0x4C,0x59,0x53,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,0x00,0x00,0xFE,
 0x00,0x44,0x49,0x47,0x49,0x4C,0x45,0x4E,0x54,0x52,0x4F,0x43,0x4B,0x53,0x00,0x12};


//全局变量,句柄
XIntc intCtrl;
XIic  iicDevice;
volatile int ibEdid;
volatile int iicIntTimes;

//用户函数
void print(char *str);
//中断服务函数
void IicHandler(void *CallBackRef);

int main()
{
    init_platform();

    print("\r\nHello World\r\n");

    iicIntTimes = 0;

    XIntc_Initialize(&intCtrl, INTC_DEVICE_ID);
    XIic_Initialize(&iicDevice, IIC_DEVICE_ID);

    XIntc_Connect(&intCtrl, IIC_IRPT_ID, IicHandler, NULL);
    XIntc_Enable(&intCtrl, IIC_IRPT_ID);

    microblaze_register_handler(XIntc_DeviceInterruptHandler, INTC_DEVICE_ID);
    microblaze_enable_interrupts();

    XIntc_Start(&intCtrl, XIN_REAL_MODE);

    /*
	 * Enable the IIC core to begin sending interrupts to the
	 * interrupt controller.
	 */
	Xil_Out32(IIC_BASEADDR + bIicIER, 0x00000026);  //Enable AAS, TxFifo empty
													//and Tx done interrupts
													//0x26 = 10 0110 (5 2 1 bit)
	Xil_Out32(IIC_BASEADDR + bIicADR, 0x000000A0);  //Set slave address for E-DDC
	Xil_Out32(IIC_BASEADDR + bIicGIE, 0x80000000);  //Enable global interrupts
	Xil_Out32(IIC_BASEADDR + bIicCR, 0x00000001);   //Enable IIC core

    while(1)
    {

    }

    return 0;
}

//IIC 中断服务
//void IicHandler(void *CallBackRef)
//{
//	if ((Xil_In32(IIC_BASEADDR + bIicISR) & bitAasFlag)) //bit 5 address as slave
//	{
//		Xil_In32(IIC_BASEADDR + bIicRX);  //Clear the receive FIFO
//	}
//	if ((Xil_In32(IIC_BASEADDR + bIicISR) & bitTxEmptyFlag)) //bit2 transmit fifo empty
//	{
//		while(((Xil_In32(IIC_BASEADDR + bIicTXOCY) & 0x0000000F) < 15) &&
//			  (ibEdid < 128))
//		{
//			Xil_Out32(IIC_BASEADDR + bIicTX, 0x000000FF & rgbEdid[ibEdid]);
//			ibEdid++;
//		}
//	}
//	if (Xil_In32(IIC_BASEADDR + bIicISR) & bitTxDoneFlag)// bit1 transmit complete(slave)
//	{
//		Xil_Out32(IIC_BASEADDR + bIicCR, 0x00000003);   //Enable IIC core + tx fifo reset
//		Xil_Out32(IIC_BASEADDR + bIicCR, 0x00000001);   //Enable IIC core
//		ibEdid = 0;
//	}
//
//	/*
//	 * Clear Interrupt Status Register in IIC core
//	 */
//	//u32 value = Xil_In32(IIC_BASEADDR + bIicISR);
//	//print("ISR Reg: ");putnum(value);print("\r\n");
//	Xil_Out32(IIC_BASEADDR + bIicISR, Xil_In32(IIC_BASEADDR + bIicISR));
//
//	iicIntTimes++;
//	//printf("iic int times is: %d \r\n", iicIntTimes);
//	//putnum(iicIntTimes);
//
//}
