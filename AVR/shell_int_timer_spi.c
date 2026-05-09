/**
 * @file main.c
 * @brief Sync Blinking between On-board LED and ENC28J60 PHY LED
 * @details 
 * - Timer0: 10ms CTC mode for 0.5s toggle interval.
 * - uart0: Interrupt-based shell for "LED" command.
 * - SPI: Hardware SPI for ENC28J60 PHY register access.
 */

#include <avr/io.h>
#define F_CPU 16000000UL

#include <util/delay.h>


#define BAUD 9600
#define UBRR_VAL 103

static volatile int command_ready = 0;
static volatile unsigned char buf[10];
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
                buf[i++] = tmp;
                uart_transmit(tmp); 
            }
}

#define PORT_SPI PORTB 
#define DDR_SPI DDRB
#define DD_MOSI DDB2
#define DD_SCK DDB1
#define DD_CS DDB0

void SPI_MasterInit(void)
{
	/* Set MOSI and SCK output, all others input */
	DDR_SPI |= (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_CS);
	/* Enable SPI, Master, set clock rate fck/16 */
	SPCR = (1<<SPE)|(1<<MSTR)|(1<<SPR0);
}

uint8_t SPI_MasterTransmit(uint8_t cData)
{
	/* Start transmission */
	SPDR = cData;
	/* Wait for transmission complete */
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}


void ENC_WriteReg(uint8_t address, uint8_t data) {
	PORT_SPI &= ~(1 << DD_CS);
	SPI_MasterTransmit(0x40 | address); //write Opcode 010, rest 5 bit is 
					    //for control register address 
	SPI_MasterTransmit(data); //8 bit data 
	PORT_SPI |= (1 << DD_CS);
}

uint8_t ENC_ReadReg(uint8_t address) {	
	volatile static uint8_t tmp;
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

	while(ENC_ReadReg(0x0a) & 0x01a);	//polling MISTAT.BUSY
	

	ENC_WriteReg(0x1f, 0x02);	//change to bank 2 for MICMD,MIRD
	ENC_WriteReg(0x12, 0x00);	//clear MICMD.MIIRD
					//
	uint8_t low_byte = ENC_ReadReg(0x18);
	uint8_t	high_byte = ENC_ReadReg(0x19);
	*data	= ((uint16_t)high_byte << 8) | low_byte;
}


void uart_transmit_Hex(uint8_t n) {
    uint8_t nibble;

    uart_transmit('0');
    uart_transmit('x');

    // 상위 4비트
    nibble = (n >> 4) & 0x0F;
    uart_transmit(nibble < 10 ? nibble + '0' : nibble - 10 + 'A');

    // 하위 4비트
    nibble = n & 0x0F;
    uart_transmit(nibble < 10 ? nibble + '0' : nibble - 10 + 'A');
}

void Display_Revision(void) {
    uint8_t rev;

    ENC_WriteReg(0x1F, 0x03); 

    // 2. EREVID(0x12) 레지스터 읽기
    // 0x12는 ETH 레지스터이므로 Dummy 없이 2번의 SPI 전송으로 끝남
    rev = ENC_ReadReg(0x12);

    // 3. 읽어온 값을 uart로 출력 (예: "Rev: 06")
    uart_transmit_Hex(rev); // HEX 형태로 출력하는 함수가 있다고 가정
    uart_transmit('\n');
}

#include <avr/interrupt.h>

volatile uint8_t count = 0;
volatile uint8_t is_blinking = 1; 
volatile uint8_t phy_update_pending = 0;
volatile uint8_t toggle = 1;


ISR(TIMER0_COMPA_vect) {
    if (!is_blinking) return;

    count++;
    if (count >= 50) { // 0.01s * 50 = 0.5s
        PINB = (1 << PINB7); 

	phy_update_pending = 1;
        
        count = 0;
    }
}
void setup_timer() {
    // CTC mode 
    TCCR0A |= (1 << WGM01);
    
    // prescaler 1024. 
    TCCR0B |= (1 << CS02) | (1 << CS00);
    
    // enable interrupt 
    TIMSK0 |= (1 << OCIE0A);
    
    // TOP value for 0.01. 16MHz/(1024*100)
    OCR0A = 156; 
    
    sei(); // enable global interrupt mode 
}

int main(void) {
    uart_init();
    DDRB |= (1 << DDB7); 
    setup_timer();
    SPI_MasterInit();
    ENC_WritePhy(0x14, 0x3990);	
    
	while (1) {
		if (command_ready) {
			if (my_strcmp(buf, "LED") == 0) {
				is_blinking = !is_blinking;
				if (!is_blinking) {
			            PORTB &= ~(1 << PB7);
				    ENC_WritePhy(0x14, 0x3990);

				    count = 0;
				    phy_update_pending = 0;
				    toggle = 1;
		                }
			
		            }
			else if(my_strcmp(buf, "REV") == 0) {
				Display_Revision();
			}
			    command_ready = 0;
		    } //command ready end 

		if(phy_update_pending) {
			if(toggle) {
				ENC_WritePhy(0x14, 0x3880);
		                toggle = 0;					
			}
			else {
				ENC_WritePhy(0x14, 0x3990);
				toggle = 1;
			}
			phy_update_pending = 0;
		} //phy pending ending 
	} //while end
}

