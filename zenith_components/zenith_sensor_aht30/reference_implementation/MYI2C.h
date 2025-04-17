#ifndef _MYI2C_h_
#define _MYI2C_h_

#define ARM32   //�������ΪARM����,ע��ΪC51����

#ifdef  ARM32   
#include "ch32f20x.h"

#define   IIC_SDA_PORT   GPIOB
#define   SDA_Pin    GPIO_Pin_7

#define   IIC_SCL_PORT   GPIOB
#define   SCL_Pin    GPIO_Pin_6

#else 
#include "STC12C5A60S2.h"

#define IIC_SDA_PIN		  P03
#define IIC_SCL_PIN		  P02

#endif
//�û��޸���
#define MYI2C_delay_us_cnt   16//�ӳ�1΢�룬����ļ����������MCU����

#define MYI2C_Tick           10//��ʱ����ʱ�䣬��λ:���룬����ݶ�ʱ����ʱ�����ã���ʱ����ʱ�����÷�Χ1-100ms
#define MYI2C_Buffer_Size    20//���ջ��������С������ȡ�ֽ���


//�ڲ����壬���������޸�
#define Wait_Ack_time        199//�ȴ�ACKӦ��ʱ�䣬��λ:΢��
#define MinReadTim           500//��ȡ��������С���ʱ�䣬��λ:����
#define PowerOnTim           10//�ϵ��ӳ�10����
#define MeasureTim           150//�ȴ���������ӳ�150����

#define SDA_OUT  0  //SDA����Ϊ���
#define SDA_IN   1  //SDA����Ϊ����
#define SCL_OUT  2  //SCL����Ϊ���

#define SDA_R    0  //��ȡSDA����
#define SCL_H    1
#define SCL_L    2
#define SDA_H    3
#define SDA_L    4

#define SENSOR_IDLE      0  //
#define SENSOR_MEASURE   1  //
#define SENSOR_COMPLETE  2  //

#define MYI2C_SCK_OD_Mode   MYI2C_GPIO_MODE(SCL_OUT)
#define MYI2C_SDA_OD_Mode   MYI2C_GPIO_MODE(SDA_OUT)
#define MYI2C_SDA_IN_Mode   MYI2C_GPIO_MODE(SDA_IN)

#define MYI2C_SCK_Clr()  MYI2C_GPIO_DATA(SCL_L);MYI2C_Delay_us(2)
#define MYI2C_SCK_Set()  MYI2C_GPIO_DATA(SCL_H);MYI2C_Delay_us(4)

#define MYI2C_SDA_Clr()  MYI2C_GPIO_DATA(SDA_L);MYI2C_Delay_us(2)
#define MYI2C_SDA_Set()  MYI2C_GPIO_DATA(SDA_H);MYI2C_Delay_us(2)

#define I2C_SDA_PIN		  P03
#define I2C_SCL_PIN		  P02

typedef struct
{

	unsigned char Adrr;
	unsigned int  timcnt;
	unsigned char ErrFlag;
	unsigned char Step;	
	unsigned int  SetRTim;

	unsigned char SendByte[MYI2C_Buffer_Size];
	unsigned char ReadByte[MYI2C_Buffer_Size];	
  float RH;//ʪ��
	float T;//�¶�
}MYI2C_Struct;

extern  MYI2C_Struct SENx;

/* Exported functions ------------------------------------------------------- */
void  MYI2C_Init(MYI2C_Struct *pst,unsigned int ReadTimMS,unsigned char xAddr);
void  MYI2C_Handle(MYI2C_Struct *pst);
/* Private functions ---------------------------------------------------------*/
void MYI2C_Delay_us(unsigned long nTim);
void MYI2C_IIC_Start(void);
void MYI2C_IIC_Stop(void);
void MYI2C_IIC_Ack(unsigned char ack);
unsigned char MYI2C_IIC_Wait_Ack(unsigned int wait_time);
void MYI2C_Write_IIC_Byte(unsigned char dat);
unsigned char MYI2C_Read_IIC_Byte(void);
//void  MYI2C_Reset(void);
unsigned char MYI2C_READ_FUNC (MYI2C_Struct *pst,unsigned char device_addr,unsigned char register_addr,unsigned char *pDat,unsigned char len);
unsigned char MYI2C_WRITE_FUNC(MYI2C_Struct *pst,unsigned char device_addr,unsigned char register_addr,unsigned char *pDat,unsigned char len);

void MYI2C_GPIO_MODE(unsigned char TYP);
unsigned char MYI2C_GPIO_DATA(unsigned char TYP);
unsigned char  CheckCrc8(unsigned char *pDat,unsigned char Lenth);

#endif //_MYI2C_h_
