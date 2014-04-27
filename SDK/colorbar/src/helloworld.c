#include <stdio.h>
#include "platform.h"
#include "xparameters.h"
#include "xintc.h"
#include "xaxivdma.h"
#include "xil_io.h"


#define NUMBER_OF_READ_FRAMES	10
#define DELAY_TIMER_COUNTER	    1

#define VERT         900
#define HORI         1600
#define PBIT         4

XAxiVdma AxiVdma;
XIntc Intc;

static XAxiVdma_DmaSetup ReadCfg;
static XAxiVdma_DmaSetup WriteCfg;

static int ReadDone;
static int ReadError;
static int WriteDone;
static int WriteError;


void print(char *str);
void genColorBar(u32 Addr);

static void ReadCallBack(void *CallbackRef, u32 Mask);
static void ReadErrorCallBack(void *CallbackRef, u32 Mask);
static void WriteCallBack(void *CallbackRef, u32 Mask);
static void WriteErrorCallBack(void *CallbackRef, u32 Mask);

int main()
{
    init_platform();

    print("Hello World\r\n");

    int Status;

	Xil_Out32(XPAR_AXI_HDMI_0_BASEADDR, 1600*900);
	u32 ResolutionValue;
	ResolutionValue = Xil_In32(XPAR_AXI_HDMI_0_BASEADDR);
	printf("HDMI Base Value is: 0x%x \r\n", ResolutionValue);


    XAxiVdma_Config *Config;
    XAxiVdma_FrameCounter FrameCfg;

    Config = XAxiVdma_LookupConfig(XPAR_AXI_VDMA_0_DEVICE_ID);
    printf("Config point to 0x%x \r\n", Config);
	if (!Config) {
		printf(
		    "No video DMA found for ID %d\r\n", XPAR_AXI_VDMA_0_DEVICE_ID);

		return XST_FAILURE;
	}

    Status = XAxiVdma_CfgInitialize(&AxiVdma, Config, Config->BaseAddress);
    printf("VDMA baseaddr: 0x%x \r\n",Config->BaseAddress);
    printf("Configuration Initialization Status is: %d ", Status);
    if(Status == XST_FAILURE) print("XST_FAILURE\r\n");

    Status = XAxiVdma_SetFrmStore(&AxiVdma, NUMBER_OF_READ_FRAMES, XAXIVDMA_READ);
    printf("SetFrmStore Status is: %d \r\n", Status);

    FrameCfg.ReadFrameCount = NUMBER_OF_READ_FRAMES;
    FrameCfg.ReadDelayTimerCount = DELAY_TIMER_COUNTER;
    FrameCfg.WriteFrameCount = 1;//就算没有wrchannel，也要写个数，不然setfrmcounter 过不了




    Status = XAxiVdma_SetFrameCounter(&AxiVdma, &FrameCfg);
    printf("SetFrameCounter Status is: %d \r\n", Status);
    //It can be ignored if status = 19, it is wrong with init WeChannel


    //Read setup
    XAxiVdma * InstancePtr = &AxiVdma;
    int Index;
    u32 Addr;


    ReadCfg.VertSizeInput = VERT;
    ReadCfg.HoriSizeInput = HORI*PBIT;

    ReadCfg.Stride = 6400;
    ReadCfg.FrameDelay = 0;
    ReadCfg.EnableCircularBuf = 1;
    ReadCfg.EnableSync = 0;
    ReadCfg.PointNum = 0;
    ReadCfg.EnableFrameCounter = 0;
    ReadCfg.FixedFrameStoreAddr = 0; //not parking
    Status = XAxiVdma_DmaConfig(InstancePtr, XAXIVDMA_READ, &ReadCfg);
    printf("RdChannel Dma Config Status is: %d \r\n", Status);

	/* Initialize buffer addresses
	 *
	 * These addresses are physical addresses
	 */
   	Addr = 0xc5000000;
	for(Index = 0; Index < NUMBER_OF_READ_FRAMES; Index++) {
		ReadCfg.FrameStoreStartAddr[Index] = Addr;

		Addr += VERT * PBIT * HORI;
	}

	/* Set the buffer addresses for transfer in the DMA engine
	 * The buffer addresses are physical addresses
	 */
	Status = XAxiVdma_DmaSetBufferAddr(InstancePtr, XAXIVDMA_READ,
			ReadCfg.FrameStoreStartAddr);
	printf("DmaSetBufferAddr Status is: %d \r\n", Status);


	for(Index=0; Index < NUMBER_OF_READ_FRAMES; Index++)
	{
		Addr = ReadCfg.FrameStoreStartAddr[Index];
		genColorBar(Addr);
	}



    //setup interrupt intc
    Status = XIntc_Initialize(&Intc, XPAR_INTC_0_DEVICE_ID);
    printf("XIntc Init Status is: %d \r\n", Status);

    Status = XIntc_Connect(&Intc, XPAR_MICROBLAZE_0_INTC_AXI_VDMA_0_MM2S_INTROUT_INTR,
    	         (XInterruptHandler)XAxiVdma_ReadIntrHandler, &AxiVdma);
    printf("VDMA RdInt Handle Connect Status is: %d \r\n", Status);

    Status = XIntc_Connect(&Intc, XPAR_MICROBLAZE_0_INTC_AXI_VDMA_0_S2MM_INTROUT_INTR,
    	         (XInterruptHandler)XAxiVdma_WriteIntrHandler, &AxiVdma);
    printf("VDMA WrInt Handle Connect Status is: %d \r\n", Status);

    Status = XIntc_Start(&Intc, XIN_REAL_MODE);
    printf("XIntc Start Status is: %d \r\n", Status);

	/* Register callback functions */
	XAxiVdma_SetCallBack(&AxiVdma, XAXIVDMA_HANDLER_GENERAL, ReadCallBack,
	    (void *)&AxiVdma, XAXIVDMA_READ);

	XAxiVdma_SetCallBack(&AxiVdma, XAXIVDMA_HANDLER_ERROR,
	    ReadErrorCallBack, (void *)&AxiVdma, XAXIVDMA_READ);

	XAxiVdma_SetCallBack(&AxiVdma, XAXIVDMA_HANDLER_GENERAL,
	    WriteCallBack, (void *)&AxiVdma, XAXIVDMA_WRITE);

	XAxiVdma_SetCallBack(&AxiVdma, XAXIVDMA_HANDLER_ERROR,
	    WriteErrorCallBack, (void *)&AxiVdma, XAXIVDMA_WRITE);

	Status = XAxiVdma_DmaStart(InstancePtr, XAXIVDMA_READ);
	printf("VDMA Start Status is: %d \r\n", Status);

	XAxiVdma_IntrEnable(&AxiVdma, XAXIVDMA_IXR_ALL_MASK, XAXIVDMA_READ);


	printf("RdChannel Base is: %x \r\n", AxiVdma.ReadChannel.ChanBase);



	return 0;
}





//interrupt handle
static void ReadCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
		ReadDone += 1;
	}
}


static void ReadErrorCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_ERROR_MASK) {
		ReadError += 1;
	}
}


static void WriteCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_FRMCNT_MASK) {
		WriteDone += 1;
	}
}


static void WriteErrorCallBack(void *CallbackRef, u32 Mask)
{

	if (Mask & XAXIVDMA_IXR_ERROR_MASK) {
		WriteError += 1;
	}
}


void genColorBar(u32 Addr)
{
	u32 offset = 0;
	for(offset = 0; offset < VERT * HORI ; )
	{
		Xil_Out32LE(Addr+4*offset, 0x000000ff);
		offset++;
	}

	for(offset = VERT * HORI * 1/4; offset < VERT * HORI ; )
	{
		Xil_Out32LE(Addr+4*offset, 0x0000ff00);
		offset++;
	}


	for(offset = VERT * HORI * 1/2; offset < VERT * HORI ; )
	{
		Xil_Out32LE(Addr+4*offset, 0x00ff0000);
		offset++;
	}
}








