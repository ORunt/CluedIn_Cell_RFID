#include "main.h"
#include <stdio.h>
#include <string.h>

//#define PROGRAMABLE_CARDS

#define FLASH_PAGE_SIZE         ((uint32_t)0x00000400)   /* FLASH Page Size */
#define FLASH_USER_START_ADDR   ((uint32_t)0x08006000)   /* Start @ of user Flash area */
#define FLASH_USER_END_ADDR     ((uint32_t)0x08006400)   /* End @ of user Flash area */

#define ERR_INVALID_CARD        0x01
#define ERR_NO_SPACE						0x02
#define ERR_NVM 								0x03

#define SUCCESS_VALID_CARD      0x01
#define SUCCESS_MASTER_CARD     0x02

#define OUT_ERR_LED							0x0001
#define OUT_SUCCESS_LED         0x0002
#define OUT_MAG_LOCK						0x0004

void USART1_Init(void);
void LEDS_init(void);

const char card_master[] = "0000F4CEAB91";	// Card 1
const char card_OG[]     = "01001158BCF4";	// Card 2
//const char jason_access[]= "30780534D7AE";  

const char * list_of_cards[] = {card_master, card_OG};

static void delay_long(uint8_t len)
{
	uint32_t delay_counter_outside, delay_counter_inside;
	
	for(delay_counter_outside = 0; delay_counter_outside < len; delay_counter_outside++)
		for(delay_counter_inside = 0; delay_counter_inside < 300000; delay_counter_inside++);
}

static void BlinkLed(uint8_t blink_num, uint8_t led)
{
	uint8_t i;
  uint8_t maglock_present = 0;
  
  if (led & OUT_MAG_LOCK)
  {
    led &= ~OUT_MAG_LOCK;
    maglock_present = 1;
  }
	
	for(i=0; i < blink_num; i++)
	{
    if (blink_num == 1)
    {
      GPIOA->ODR |= led;
      if (maglock_present)
        GPIOA->ODR &= ~OUT_MAG_LOCK;
      delay_long(15);
      GPIOA->ODR &= ~led;
      if (maglock_present)
        GPIOA->ODR |= OUT_MAG_LOCK;
    }
    else
    {
      delay_long(15);
      GPIOA->ODR |= led;
      delay_long(15);
      GPIOA->ODR &= ~led;
    }
	}
}

#ifndef PROGRAMABLE_CARDS

static uint8_t CheckCardPresent(uint8_t * card_num)
{
	uint8_t i;
  uint8_t num_of_cards = sizeof(list_of_cards)/sizeof(char*);
  
  for(i = 0; i < num_of_cards; i++)
  {
    if (!memcmp(list_of_cards[i], card_num, 12))
      return 1;
  }

	return 0;	// card not found
}

#else

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
		BlinkLed(SUCCESS_MASTER_CARD, OUT_SUCCESS_LED);
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
			BlinkLed(ERR_NO_SPACE, OUT_ERR_LED);
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
			BlinkLed(ERR_NVM, OUT_ERR_LED);
			break;
		}
	}
	FLASH_Lock();
}

#endif

int main(void)
{
	char uart_buf[256] = { 0 };
	uint8_t i = 0;
	uint8_t master_mode = 0;
	
	LEDS_init();
	USART1_Init();
  
  GPIOA->ODR |= OUT_MAG_LOCK; // Must be normally powered

	while (1)
	{
		while(USART_GetFlagStatus(USART1,USART_FLAG_RXNE) == RESET);
		uart_buf[i] = (char)USART_ReceiveData(USART1);
		if(uart_buf[i++] == 0x03)
		{
			USART_Cmd(USART1,DISABLE);
      
#ifdef PROGRAMABLE_CARDS
			if (master_mode)
				WriteNewCard((uint8_t *)uart_buf + 1);
			
			master_mode = CheckForMasterCard((uint8_t *)uart_buf + 1);
#endif
			
			if(CheckCardPresent((uint8_t *)uart_buf + 1) && (master_mode == 0))
        BlinkLed(SUCCESS_VALID_CARD, OUT_MAG_LOCK | OUT_SUCCESS_LED);
      else
        BlinkLed(ERR_INVALID_CARD, OUT_ERR_LED);
      
			i = 0;
			delay_long(20);
			USART_Cmd(USART1,ENABLE);
		}
	}
}


void LEDS_init(void)
{
  GPIO_InitTypeDef GPIO_InitStructure = {0};

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
  
  GPIO_InitStructure.GPIO_Pin = OUT_ERR_LED | OUT_SUCCESS_LED | OUT_MAG_LOCK;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  GPIOA->ODR &= ~(OUT_ERR_LED | OUT_SUCCESS_LED | OUT_MAG_LOCK); // Make sure these puppies are off
}

void USART1_Init(void)	//this uart uses pin PA2 = TX and PA3 = RX
{
	USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);

  //GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);

  //Configure USART1 pins:  Rx and Tx ----------------------------
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_3;
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
  USART_InitStructure.USART_Mode = USART_Mode_Rx;
  USART_Init(USART1, &USART_InitStructure);

  USART_Cmd(USART1,ENABLE);
}

