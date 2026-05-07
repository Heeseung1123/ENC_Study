/**
 * @file main.c
 * @brief Sync Blinking between On-board LED and ENC28J60 PHY LED
 * @details 
 * - Timer0: 10ms CTC mode for 0.5s toggle interval.
 * - uart0: Interrupt-based shell for "LED" command.
 * - SPI: Hardware SPI for ENC28J60 PHY register access.
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL

#include <util/delay.h>


#define BAUD 9600
#define UBRR_VAL 103

volatile int command_ready = 0;
volatile unsigned char buf[10];
static unsigned char tmp;
static	int i = 0;

void uart_init(void) {
    // main.c:10-11
    UBRR0H = (unsigned char)(UBRR_VAL >> 8);
    UBRR0L = (unsigned char)UBRR_VAL;

//    sei();
    SREG |= (1 << 7); //enable global interrupt flag
    UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1<<RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_transmit(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

unsigned char uart_receive(void) {
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

int my_strcmp(const unsigned char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}



__attribute__ ((signal)) void USART0_RX_vect(void)  { //ISR for uart RX int
	tmp = UDR0; 	

        if (tmp == '\r' || tmp == '\n') { // 엔터 입력 시
            buf[i] = '\0'; // 문자열 종료
            uart_transmit('\r');
            uart_transmit('\n');


            i = 0; // 버퍼 인덱스 초기화
            uart_transmit('#');
	    uart_transmit(' ');
	    command_ready = 1;
        }
        else { // 일반 문자 입력 시
	       if (i < 9) {       // 경계 체크 추가
	        buf[i++] = tmp;
	        uart_transmit(tmp);
	    }
}
}


#define PORT_SPI PORTB 
#define DDR_SPI DDRB
#define DD_MOSI DDB2
#define DD_MISO DDB3
#define DD_SCK DDB1
#define DD_CS DDB0

void SPI_MasterInit(void)
{
	/* Set MOSI and SCK output, all others input */
	DDR_SPI |= (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_CS);
	DDR_SPI &= ~(1 << DD_MISO); 
	PORTB |= (1 << PORTB0);

	/* Enable SPI, Master, set clock rate fck/16 */
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR1)|(1<<SPR0);
}

uint8_t SPI_MasterTransmit(uint8_t cData)
{
	/* Start transmission */
	SPDR = cData;
	/* Wait for transmission complete */
	while(!(SPSR & (1<<SPIF)));
	uint8_t data = SPDR;
	return data;
}


void ENC_WriteReg(uint8_t address, uint8_t data) {
	PORT_SPI &= ~(1 << DD_CS);
	SPI_MasterTransmit(0x40 | address); //write Opcode 010, rest 5 bit is 
					    //for control register address 
	SPI_MasterTransmit(data); //8 bit data 
	PORT_SPI |= (1 << DD_CS);
}

uint8_t ENC_ReadReg(uint8_t address) {	
	uint8_t tmp;
	PORT_SPI &= ~(1 << DD_CS);
	SPI_MasterTransmit(0x00 | address); //setting address to read 
	SPI_MasterTransmit(0x00);         // first Dummy 
	tmp = SPI_MasterTransmit(0x00); //actual read data		
	PORT_SPI |= (1 << DD_CS);
	return tmp;
}


void ENC_WritePhy(uint8_t address, uint16_t data) {
	ENC_WriteReg(0x1f, 0x02);	//change to bank2
	ENC_WriteReg(0x14, address);      // MIREGADR에 PHY 레지스터 주소 기록
	ENC_WriteReg(0x16, data & 0xFF);  // MIWR 하위 바이트 기록
	ENC_WriteReg(0x17, data >> 8);    // MIWR 상위 바이트 기록
	_delay_us(200);                   // PHY 처리 대기
}

void ENC_ReadPhy(uint8_t address, uint16_t* data) {
	ENC_WriteReg(0x1f, 0x02);
	ENC_WriteReg(0x14, address);
	ENC_WriteReg(0x12, 0x01);	//set bit to MICMD.MIIRD
	ENC_WriteReg(0x1f, 0x03);	//change to bank 3 for MISTAT 

	while(ENC_ReadReg(0x0a) & 0x01);	//polling MISTAT.BUSY
	

	ENC_WriteReg(0x1f, 0x02);	//change to bank 2 for MICMD,MIRD
	ENC_WriteReg(0x12, 0x00);	//clear MICMD.MIIRD
					//
	uint8_t low_byte = ENC_ReadReg(0x18);
	uint8_t	high_byte = ENC_ReadReg(0x19);
	*data	= ((uint16_t)high_byte << 8) | low_byte;
}

void ENC_SoftReset(void) {
    PORT_SPI &= ~(1 << DD_CS);
    SPI_MasterTransmit(0xFF);
    PORT_SPI |= (1 << DD_CS);
    _delay_ms(50); // 리셋 후 안정화 시간 필수
}

