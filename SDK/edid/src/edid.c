#include "edid.h"

//IIC ÖÐ¶Ï·þÎñ
void IicHandler(void *CallBackRef)
{
	if ((Xil_In32(IIC_BASEADDR + bIicISR) & bitAasFlag)) //bit 5 address as slave
	{
		Xil_In32(IIC_BASEADDR + bIicRX);  //Clear the receive FIFO
	}
	if ((Xil_In32(IIC_BASEADDR + bIicISR) & bitTxEmptyFlag)) //bit2 transmit fifo empty
	{
		while(((Xil_In32(IIC_BASEADDR + bIicTXOCY) & 0x0000000F) < 15) &&
			  (ibEdid < 128))
		{
			Xil_Out32(IIC_BASEADDR + bIicTX, 0x000000FF & rgbEdid[ibEdid]);
			ibEdid++;
		}
	}
	if (Xil_In32(IIC_BASEADDR + bIicISR) & bitTxDoneFlag)// bit1 transmit complete(slave)
	{
		Xil_Out32(IIC_BASEADDR + bIicCR, 0x00000003);   //Enable IIC core + tx fifo reset
		Xil_Out32(IIC_BASEADDR + bIicCR, 0x00000001);   //Enable IIC core
		ibEdid = 0;
	}

	/*
	 * Clear Interrupt Status Register in IIC core
	 */
	//u32 value = Xil_In32(IIC_BASEADDR + bIicISR);
	//print("ISR Reg: ");putnum(value);print("\r\n");
	Xil_Out32(IIC_BASEADDR + bIicISR, Xil_In32(IIC_BASEADDR + bIicISR));

	iicIntTimes++;
	//printf("iic int times is: %d \r\n", iicIntTimes);
	//putnum(iicIntTimes);

}


void EdidInit(XIntc* pIntCtrl)
{
	 XIntc_Connect(pIntCtrl, IIC_IRPT_ID, IicHandler, NULL);
	 XIntc_Enable(pIntCtrl, IIC_IRPT_ID);
}
