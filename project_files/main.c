#include "main.h"
#include <stdio.h>
#include <string.h>

#define FLASH_PAGE_SIZE         ((uint32_t)0x00000400)   /* FLASH Page Size */
#define FLASH_USER_START_ADDR   ((uint32_t)0x08006000)   /* Start @ of user Flash area */
#define FLASH_USER_END_ADDR     ((uint32_t)0x08006400)   /* End @ of user Flash area */

#define ERR_MASTER_CARD					0x01
#define ERR_NO_SPACE						0x02
#define ERR_NVM 								0x03

#define OUT_ERR_LED							0x0001
#define OUT_MAG_LOCK						0x0002

void USART2_Init(void);
void LEDS_init(void);

const char card_master[] = "0000F4CEAB91";	// Card 1
const char card_OG[]     = "01001158BCF4";	// Card 2

static void delay_long(uint8_t len)
{
	uint32_t delay_counter_outside, delay_counter_inside;
	
	for(delay_counter_outside = 0; delay_counter_outside < len; delay_counter_outside++)
		for(delay_counter_inside = 0; delay_counter_inside < 300000; delay_counter_inside++);
}

static void ErrorToLeds(uint8_t err_type)
{
	uint8_t i;
	
	for(i=0; i < err_type; i++)
	{
		delay_long(2);
		GPIOB->ODR = OUT_ERR_LED;
		delay_long(2);
		GPIOB->ODR = 0x00;
	}
}

static uint8_t CheckCardPresent(uint8_t * card_num)
{
	uint32_t address = FLASH_USER_START_ADDR;
	
	if (!memcmp(card_OG, card_num, 12))	// Check OG (the original) card first
		return 1;
	
	while(address < FLASH_USER_END_ADDR)
	{
		if(!memcmp((uint8_t *)address, card_num, 12))
			return 1;	// card found
		address += 12;
	}
	return 0;	// card not found
}

static uint8_t CheckForMasterCard(uint8_t * card_num)
{
	if (!memcmp(card_master, card_num, 12))	// Check master card first
	{
		ErrorToLeds(ERR_MASTER_CARD);
		return 1;	// Master Card found
	}
	return 0;
}

static uint32_t FindEmptySlot(void)
{
	uint32_t address = FLASH_USER_START_ADDR;
	while(*((uint32_t *)address) != 0xFFFFFFFF)
	{
		if (address > FLASH_USER_END_ADDR)
		{
			ErrorToLeds(ERR_NO_SPACE);
			return 0;
		}
		address += 12;
	}
	return address;
}

static void WriteNewCard(uint8_t * card_num)
{
	uint32_t address = FindEmptySlot();
	uint8_t i;
	
	if(address == 0)
		return;
	
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
	for(i = 0; i < 3; i++)
	{
		if (FLASH_ProgramWord(address, (uint32_t)*((uint32_t *)card_num)) == FLASH_COMPLETE)
		{
			address += 4;
			card_num += 4;
		}
		else
		{
			ErrorToLeds(ERR_NVM);
			break;
		}
	}
	FLASH_Lock();
}

int main(void)
{
	char uart_buf[14] = { 0 };
	uint8_t i = 0;
	uint8_t master_mode = 0;
	
	LEDS_init();
	USART2_Init();

	while (1)
	{
		while(USART_GetFlagStatus(USART2,USART_FLAG_RXNE) == RESET);
		uart_buf[i] = (char)USART_ReceiveData(USART2);
		if(uart_buf[i++] == 0x03)
		{
			if (master_mode)
				WriteNewCard((uint8_t *)uart_buf + 1);
			
			master_mode = CheckForMasterCard((uint8_t *)uart_buf + 1);
			
			if(CheckCardPresent((uint8_t *)uart_buf + 1) && (master_mode == 0))
			{
				GPIOB->ODR = OUT_MAG_LOCK;
				delay_long(12);
				GPIOB->ODR = 0;
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

