#include <stdio.h>
#include "platform.h"
#include "edid.h"
#include "xgpio.h"
#include "xaxivdma.h"
#include "xil_exception.h"

#define    INTC_DEVICE_ID        XPAR_MICROBLAZE_0_INTC_DEVICE_ID
#define    BUTTON_DEVICE_ID      XPAR_PUSH_BUTTONS_5BITS_DEVICE_ID
#define    LED_DEVICE_ID         XPAR_LEDS_8BITS_DEVICE_ID

//vdma macro
#define    VDMA_BASE_ADDR        XPAR_AXIVDMA_0_BASEADDR
#define    DMA_DEVICE_ID         XPAR_AXIVDMA_0_DEVICE_ID
#define    WRITE_INTR_ID         XPAR_MICROBLAZE_0_INTC_AXI_VDMA_0_S2MM_INTROUT_INTR //write: source to Atyls
#define    READ_INTR_ID          XPAR_MICROBLAZE_0_INTC_AXI_VDMA_0_MM2S_INTROUT_INTR //read: Atyls Output DVI
#define    MEM_BASE_ADDR         0xc5000000 //有待检验
#define    READ_ADDRESS_BASE	 MEM_BASE_ADDR
#define    WRITE_ADDRESS_BASE	 MEM_BASE_ADDR
#define    NUMBER_OF_READ_FRAMES	3
#define    NUMBER_OF_WRITE_FRAMES	3
#define    DELAY_TIMER_COUNTER	 1


#define FRAME_HORIZONTAL_LEN  (640*4)   /* 1920 pixels, each pixel 4 bytes */
#define FRAME_VERTICAL_LEN    480    /* 1080 pixels */

XIntc intCtrl;
XGpio pushButton;
XGpio led;
XAxiVdma vdma;

int Status;
XAxiVdma_Config *Config;
XAxiVdma_FrameCounter FrameCfg;

/* DMA channel setup
 */
static XAxiVdma_DmaSetup ReadCfg;
static XAxiVdma_DmaSetup WriteCfg;
/* Transfer statics
 */
static int ReadDone;
static int ReadError;
static int WriteDone;
static int WriteError;


void print(char *str);
void wait(u32 usec);
//VDMA ISR
static void ReadCallBack(void *CallbackRef, u32 Mask);
static void ReadErrorCallBack(void *CallbackRef, u32 Mask);
static void WriteCallBack(void *CallbackRef, u32 Mask);
static void WriteErrorCallBack(void *CallbackRef, u32 Mask);

int main()
{
    init_platform();

    print("\r\nHello World\r\n");

    WriteDone = 0;
	ReadDone = 0;
	WriteError = 0;
	ReadError = 0;
    iicIntTimes = 0;

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




//----VDMA Setup-----------------------------------------------------------------------------------------
	Config = XAxiVdma_LookupConfig(DMA_DEVICE_ID);
	if (!Config) {print("LookupConfig Failed \r\n");}

	Status = XAxiVdma_CfgInitialize(&vdma, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {print("CfgInit Failed \r\n");}

	Status = XAxiVdma_SetFrmStore(&vdma, NUMBER_OF_READ_FRAMES, XAXIVDMA_READ);
	if (Status != XST_SUCCESS) {print("Rd SetFrmSotre Failed \r\n");}

	Status = XAxiVdma_SetFrmStore(&vdma, NUMBER_OF_WRITE_FRAMES, XAXIVDMA_WRITE);
	if (Status != XST_SUCCESS) {print("Wr SetFrmSotre Failed \r\n");}

	FrameCfg.ReadFrameCount = NUMBER_OF_READ_FRAMES;
	FrameCfg.WriteFrameCount = NUMBER_OF_WRITE_FRAMES;
	FrameCfg.ReadDelayTimerCount = DELAY_TIMER_COUNTER;
	FrameCfg.WriteDelayTimerCount = DELAY_TIMER_COUNTER;

	Status = XAxiVdma_SetFrameCounter(&vdma, &FrameCfg);
	if (Status != XST_SUCCESS) {print("SetFrmCounter Failed \r\n");}

//////////////////////////////////////////////////////////////////////////////////////////////
	int Index;
	u32 Addr;
	WriteCfg.VertSizeInput = FRAME_VERTICAL_LEN;
	WriteCfg.HoriSizeInput = FRAME_HORIZONTAL_LEN;
	WriteCfg.Stride = FRAME_HORIZONTAL_LEN;
	WriteCfg.FrameDelay = 0;  /* This example does not test frame delay */
	WriteCfg.EnableCircularBuf = 1;
	WriteCfg.EnableSync = 0;  /* No Gen-Lock */
	WriteCfg.PointNum = 0;    /* No Gen-Lock */
	WriteCfg.EnableFrameCounter = 0; /* Endless transfers */
	WriteCfg.FixedFrameStoreAddr = 0; /* We are not doing parking */
	Status = XAxiVdma_DmaConfig(&vdma, XAXIVDMA_WRITE, &WriteCfg);
	if (Status != XST_SUCCESS) {print("Wr Dma Cfg Failed \r\n");}

	Addr = WRITE_ADDRESS_BASE;
	for(Index = 0; Index < NUMBER_OF_WRITE_FRAMES; Index++) {
		WriteCfg.FrameStoreStartAddr[Index] = Addr;
		Addr += FRAME_HORIZONTAL_LEN * FRAME_VERTICAL_LEN;
	}

	Status = XAxiVdma_DmaSetBufferAddr(&vdma, XAXIVDMA_WRITE,	WriteCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS) {print("Wr DmaSetBufferAddr Cfg Failed \r\n");}

//////////////////////////////////////////////////////////////////////////////////////////////////////
	ReadCfg.VertSizeInput = FRAME_VERTICAL_LEN;
	ReadCfg.HoriSizeInput = FRAME_HORIZONTAL_LEN;
	ReadCfg.Stride = FRAME_HORIZONTAL_LEN;
	ReadCfg.FrameDelay = 0;  /* This example does not test frame delay */
	ReadCfg.EnableCircularBuf = 1;
	ReadCfg.EnableSync = 0;  /* No Gen-Lock */
	ReadCfg.PointNum = 0;    /* No Gen-Lock */
	ReadCfg.EnableFrameCounter = 0; /* Endless transfers */
	ReadCfg.FixedFrameStoreAddr = 0; /* We are not doing parking */

	Status = XAxiVdma_DmaConfig(&vdma, XAXIVDMA_READ, &ReadCfg);
	if (Status != XST_SUCCESS) {print("Rd Dma Cfg Failed \r\n");}

	Addr = READ_ADDRESS_BASE;
	for(Index = 0; Index < NUMBER_OF_READ_FRAMES; Index++) {
		ReadCfg.FrameStoreStartAddr[Index] = Addr;
		Addr += FRAME_HORIZONTAL_LEN * FRAME_VERTICAL_LEN;
	}

	Status = XAxiVdma_DmaSetBufferAddr(&vdma, XAXIVDMA_READ, ReadCfg.FrameStoreStartAddr);
	if (Status != XST_SUCCESS) {print("Rd DmaSetBufferAddr Cfg Failed \r\n");}

///////////////////////////////////////////////////////////////////////////////////////////////////////
	Status = XIntc_Connect(&intCtrl, READ_INTR_ID,
		         (XInterruptHandler)XAxiVdma_ReadIntrHandler, &vdma);
	Status = XIntc_Connect(&intCtrl, WRITE_INTR_ID,
		         (XInterruptHandler)XAxiVdma_WriteIntrHandler, &vdma);

	Status = XIntc_Start(&intCtrl, XIN_REAL_MODE);
	XIntc_Enable(&intCtrl, READ_INTR_ID);
	XIntc_Enable(&intCtrl, WRITE_INTR_ID);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)XIntc_InterruptHandler,
			(void *)&intCtrl);
	Xil_ExceptionEnable();

	/* Register callback functions
	 */
	XAxiVdma_SetCallBack(&vdma, XAXIVDMA_HANDLER_GENERAL, ReadCallBack,
	    (void *)&vdma, XAXIVDMA_READ);

	XAxiVdma_SetCallBack(&vdma, XAXIVDMA_HANDLER_ERROR,
	    ReadErrorCallBack, (void *)&vdma, XAXIVDMA_READ);

	XAxiVdma_SetCallBack(&vdma, XAXIVDMA_HANDLER_GENERAL,
	    WriteCallBack, (void *)&vdma, XAXIVDMA_WRITE);

	XAxiVdma_SetCallBack(&vdma, XAXIVDMA_HANDLER_ERROR,
	    WriteErrorCallBack, (void *)&vdma, XAXIVDMA_WRITE);

	Status = XAxiVdma_DmaStart(&vdma, XAXIVDMA_WRITE);
	if (Status != XST_SUCCESS) {print("Wr DmaStart Failed \r\n");}
	Status = XAxiVdma_DmaStart(&vdma, XAXIVDMA_READ);
	if (Status != XST_SUCCESS)
	{
		print("Rd DmaStart Failed \r\n");
		printf("Status : 0x%x \r\n", Status);
	}

	XAxiVdma_IntrEnable(&vdma, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_WRITE);
	XAxiVdma_IntrEnable(&vdma, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_READ);

	if (ReadError || WriteError) {print("Rd or Wr Failed \r\n");}
//----VDMA Setup-----------------------------------------------------------------------------------------

	Xil_Out32(XPAR_AXI_HDMI_0_BASEADDR, FRAME_HORIZONTAL_LEN/4*FRAME_VERTICAL_LEN);
	u32 ResolutionValue;
	ResolutionValue = Xil_In32(XPAR_AXI_HDMI_0_BASEADDR);
	//putnum(ResolutionValue);
	xil_printf("HDMI Base Value is: 0x%x \r\n", ResolutionValue);


	XAxiVdma_Reset(&vdma, XAXIVDMA_READ);
	XAxiVdma_Reset(&vdma, XAXIVDMA_WRITE);

	while(1)
	{
		wait(2000);
//		putnum(iicIntTimes);
//		print("\r\n0x");
//		XGpio_DiscreteSet(&led, 1, WriteError);
//		print("\r\nStart\r\n0x");
//		putnum(iicIntTimes);
//		print("\r\n0x");
//		putnum(WriteError);
//		print("\r\n0x");
//		putnum(ReadError);
//		print("\r\n");
		u32 value;
		value = Xil_In32(VDMA_BASE_ADDR);
		printf("offset 0x00 is: 0x%x \r\n",value);
		value = Xil_In32(VDMA_BASE_ADDR+0x04);
		printf("offset 0x04 is: 0x%x \r\n",value);
		value = Xil_In32(VDMA_BASE_ADDR+0x30);
		printf("offset 0x30 is: 0x%x \r\n",value);
		value = Xil_In32(VDMA_BASE_ADDR+0x34);
		printf("offset 0x34 is: 0x%x \r\n",value);

		printf("ResetNotDone is %d \r\n",XAxiVdma_ResetNotDone(&vdma, XAXIVDMA_READ));
	}


    return 0;
}


void wait(u32 usec)//等待ms数
{
	volatile u32 x,y,z;
	for(z=0; z<usec;z++)
		for(x=0; x<100;x++)
			for(y=0; y<100;y++)
			{;}
}


/*****************************************************************************/
/*
 * Call back function for read channel
 *
 * This callback only clears the interrupts and updates the transfer status.
 *
 * @param	CallbackRef is the call back reference pointer
 * @param	Mask is the interrupt mask passed in from the driver
 *
 * @return	None
*
******************************************************************************/
static void ReadCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
		ReadDone += 1;
	}
}

/*****************************************************************************/
/*
 * Call back function for read channel error interrupt
 *
 * @param	CallbackRef is the call back reference pointer
 * @param	Mask is the interrupt mask passed in from the driver
 *
 * @return	None
*
******************************************************************************/
static void ReadErrorCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_ERROR_MASK) {
		ReadError += 1;
	}
}

/*****************************************************************************/
/*
 * Call back function for write channel
 *
 * This callback only clears the interrupts and updates the transfer status.
 *
 * @param	CallbackRef is the call back reference pointer
 * @param	Mask is the interrupt mask passed in from the driver
 *
 * @return	None
*
******************************************************************************/
static void WriteCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
		WriteDone += 1;
	}
}

/*****************************************************************************/
/*
* Call back function for write channel error interrupt
*
* @param	CallbackRef is the call back reference pointer
* @param	Mask is the interrupt mask passed in from the driver
*
* @return	None
*
******************************************************************************/
static void WriteErrorCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_ERROR_MASK) {
		WriteError += 1;
	}
}
