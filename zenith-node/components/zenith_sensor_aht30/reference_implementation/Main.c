
#include <stdio.h>
#include "MYI2C.h"

int main( void )
{
	
	MYI2C_Init(&SENx,2000,0x38);    //2000:读取数据周期2S;   0x38:AHT20地址
	
	while(1) {


		 MYI2C_Handle(&SENx);   //不使用死延时则定时10ms调用一次该函数
	//	 printf("RH = %0.2f",SENx.RH);
	//	 printf("T = %0.2f\n",SENx.T);						
		 MYI2C_Delay_us(10500); //根据MCU调整为10ms延时
	}	 
	 
}