
#include <stdio.h>
#include "MYI2C.h"

int main( void )
{
	
	MYI2C_Init(&SENx,2000,0x38);    //2000:��ȡ��������2S;   0x38:AHT20��ַ
	
	while(1) {


		 MYI2C_Handle(&SENx);   //��ʹ������ʱ��ʱ10ms����һ�θú���
	//	 printf("RH = %0.2f",SENx.RH);
	//	 printf("T = %0.2f\n",SENx.T);						
		 MYI2C_Delay_us(10500); //����MCU����Ϊ10ms��ʱ
	}	 
	 
}