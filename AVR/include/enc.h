/**
 * @file main.c
 * @brief Sync Blinking between On-board LED and ENC28J60 PHY LED
 * @details 
 * - Timer0: 10ms CTC mode for 0.5s toggle interval.
 * - uart0: Interrupt-based shell for "LED" command.
 * - SPI: Hardware SPI for ENC28J60 PHY register access.
 */
#ifndef ENC_H
#define ENC_H
#include <avr/io.h>
#define F_CPU 16000000UL

#include <util/delay.h>


#define BAUD 9600
#define UBRR_VAL 103

extern volatile int command_ready;
extern unsigned char *buf;

void ENC_SoftReset(void);
void uart_init(void);
void uart_transmit(unsigned char data);
unsigned char uart_receive(void);
int my_strcmp(const unsigned char* s1, const char* s2);



#define PORT_SPI PORTB 
#define DDR_SPI DDRB
#define DD_MOSI DDB2
#define DD_SCK DDB1
#define DD_CS DDB0

void SPI_MasterInit(void);
uint8_t SPI_MasterTransmit(uint8_t cData);
void ENC_WriteReg(uint8_t address, uint8_t data);
uint8_t ENC_ReadReg(uint8_t address);
void ENC_WritePhy(uint8_t address, uint16_t data);
void ENC_ReadPhy(uint8_t address, uint16_t* data);


#include <avr/interrupt.h>


#endif 

