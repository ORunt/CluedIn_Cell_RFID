#include "main.h"
#include <stdio.h>
#include <string.h>

void USART2_Init(void);
void LEDS_init(void);

int main(void)
{
	char cam[14] = { 0 };
	char card_id_1[] = "0000F4CEAB91";
	char card_id_2[] = "01001158BCF4";
	char card_id_3[] = "307E785C204A";
	uint8_t i = 0;
	LEDS_init();
	USART2_Init();

	while (1)
	{
		while(USART_GetFlagStatus(USART2,USART_FLAG_RXNE) == RESET);
		cam[i] = (char)USART_ReceiveData(USART2);
		if(cam[i++] == 0x03)
		{
			if(!memcmp(card_id_1, cam + 1, 12))
			{
				GPIOB->ODR = 0x01;
			}
			if(!memcmp(card_id_2, cam + 1, 12))
			{
				GPIOB->ODR = 0x02;
			}
			if(!memcmp(card_id_3, cam + 1, 12))
			{
				GPIOB->ODR = 0x04;
			}
			i = 0;
		}
	}
}

void LEDS_init(void)
{
	RCC->AHBENR |= 0x040000;	// This must be changed later on the actual puzzle
	GPIOB->MODER = 0x5555;
	GPIOB->ODR = 0x00;
}

void USART2_Init(void)	//this uart uses pin PA2 = TX and PA3 = RX
{
	USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);

  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);

  //Configure USART2 pins:  Rx and Tx ----------------------------
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2 | GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  USART_InitStructure.USART_BaudRate = 9600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART2, &USART_InitStructure);

  USART_Cmd(USART2,ENABLE);
}

