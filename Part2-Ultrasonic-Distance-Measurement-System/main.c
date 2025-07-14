#include "stm32l476xx.h"
#include <stdio.h>

#define TRIG_PIN    5		// Pin PB5
#define ECHO_PIN		6		// Pin PB6
#define TX_PIN 			2 	// Pin PA2

void configure_trig_pin(){
  // Enable the clock to GPIO Port B
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
		
	// GPIO Mode: Input(00), Output(01), AlterFunc(10), Analog(11, reset)
	GPIOB->MODER &= ~(3UL<<(2*TRIG_PIN));
	GPIOB->MODER |=  (1UL<<(2*TRIG_PIN));     // Output(01)
	
	// GPIO Speed: Low speed (00), Medium speed (01), Fast speed (10), High speed (11)
	GPIOB->OSPEEDR &= ~(3UL<<(2*TRIG_PIN));
	GPIOB->OSPEEDR |=  (3UL<<(2*TRIG_PIN));  	// High speed
	
	// GPIO Output Type: Output push-pull (0, reset), Output open drain (1) 
	GPIOB->OTYPER &= ~(1UL<<TRIG_PIN);      	// Push-pull
}

void configure_echo_pin(){
	// GPIOB clock has already been enabled
	
	// GPIO Mode: Input(00), Output(01), AlterFunc(10), Analog(11, reset)
	GPIOB->MODER &= ~(3UL<<(2*ECHO_PIN));			// Input (00)
	
	// GPIO Push-Pull: No pull-up, pull-down (00), Pull-up (01), Pull-down (10), Reserved (11)
	GPIOB->PUPDR &= ~(3UL<<(2*ECHO_PIN));  		// No pull-up, no pull-down
}

void TX_Init(void) {
	// Enable the clock to GPIO Port A
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	
  // Set PA2 to alternate function (AF7 = USART2)
  // Setting TX pin to alternate function (10)
	GPIOA->MODER &= ~(3UL<<(2*TX_PIN));
	GPIOA->MODER |= (2UL<<(2*TX_PIN));
	
	//pin 2, tx
	GPIOA->AFR[0] &= ~(15UL << 4*TX_PIN); // clearing pin 2 AltFunc bits.
	GPIOA->AFR[0] |= (7UL << 4*TX_PIN); 	// setting pin 2 altfunc to Altfunc 7.
}

void USART2_Init(void) {
	 
	// Baud: 115200, Data: 8 bits, Parity: None, Stop: 1 bit, Flow Control: None

	RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN; //ENABLE USART2 clocks 
  
  USART2->CR1 &= ~USART_CR1_UE; // Disable USART2 before configuration

  // Set baud rate: 4 MHz / 115200 = 34.72 ? rounded to 35
  USART2->BRR = 35;

  USART2->CR1 |= USART_CR1_TE; // Enable Transmitter

  USART2->CR1 |= USART_CR1_UE; // Enable USART2
}

void USART2_Transmit(char *str) {
	while (*str) {
		while (!(USART2->ISR & USART_ISR_TXE)); // Wait until transmit buffer empty
		USART2->TDR = *str++;
	}
  while (!(USART2->ISR & USART_ISR_TC));  	// Wait until transmission complete
}

void TIM4_Init(void) {
	// Enable the timer clock
  RCC->APB1ENR1 	|= RCC_APB1ENR1_TIM4EN; 	// Enable TIMER clock
	
	TIM4->PSC = 4 - 1;		// 4 MHz / 4 = 1 MHz --> 1 microsecond
	TIM4->ARR = 0xFFFF;		// Max value that counter goes up to (65535)
	TIM4->CNT = 0;				// Start counter at 0
	TIM4->CR1  |= TIM_CR1_CEN; // Enable counter and count up
}

void send_trig_pulse(void) {
	uint32_t start_time;
	
	GPIOB->ODR &= ~(1U<<TRIG_PIN);		// Ensure LOW
	start_time = TIM4->CNT;
	
	while ((TIM4->CNT - start_time) < 2);		// Wait 2 microseconds
	
	GPIOB->ODR |= (1U<<TRIG_PIN);		// Set to HIGH
	start_time = TIM4->CNT;
	while ((TIM4->CNT - start_time) < 10);	// Wait 10 microseconds
	
	GPIOB->ODR &= ~(1U<<TRIG_PIN);		// Go back to LOW
}

int main(void){
	configure_trig_pin();
	configure_echo_pin();
	TX_Init();
  USART2_Init();
	TIM4_Init();
	
	char buffer[64];
	
	USART2_Transmit("Ultrasonic Sensor is ready\r\n");
	
  // Dead loop & program hangs here
	while(1){
		send_trig_pulse();
		
		// Wait for echo to go HIGH
		uint32_t timeout = TIM4->CNT;
		while ((GPIOB->IDR & (1<<ECHO_PIN)) == 0) {	// Continue looping while still LOW
			if ((TIM4->CNT - timeout) > 30000) break;	// Timeout of about 30 ms
		}
		
		uint32_t start = TIM4->CNT;
		
		// Wait for echo to go LOW
		while((GPIOB->IDR & (1<<ECHO_PIN)) != 0);		// Continue looping while still HIGH
		
		uint32_t end = TIM4->CNT;
		
		// Calculate duration that echo is HIGH -> proportional to round-trip time of sound wave
		uint32_t time = (end >= start) ? (end - start) : (0xFFFF - start + end);	// In microseconds
		
		// Speed of sound ~ 343 m/s = 0.0343 cm/us --> 1 cm is about 29.1 us round-trip
		unsigned long distance = time/58;		// One-way distance in cm
		
		sprintf(buffer, "Distance: %lu cm\r\n", distance);
		USART2_Transmit(buffer);
		
		// Delay about 500 ms using CNT
		start = TIM4->CNT;
		while ((TIM4->CNT - start) < 500000);
	}
}
