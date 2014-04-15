#ifndef __USER_H_
#define __USER_H_

void wait(u32 usec)//µÈ´ýmsÊý
{
	volatile u32 x,y,z;
	for(z=0; z<usec;z++)
		for(x=0; x<100;x++)
			for(y=0; y<100;y++)
			{;}
}



#endif

