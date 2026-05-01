/**
 * @file main.c
 * @brief UART Interrupt-based LED Control System
 * * @details 
 * This program demonstrates asynchronous serial communication using UART interrupts.
 * While the main loop toggles an LED (PB7) in the background, the ISR captures 
 * user commands to modify the pin's hardware configuration (DDRB).
 */

#include <avr/io.h>


#define F_CPU 16000000UL
#define BAUD 9600
#define UBRR_VAL 103

static volatile int command_ready = 0;
static volatile unsigned char buf[10];
static int i;
static unsigned char tmp;

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



__attribute__ ((signal)) void USART0_RX_vect(void)  { //ISR for UART RX int
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
		


int main(void) {
	volatile unsigned int loop_count = 0; 
	DDRB |= (1 << DDB7); // LED3(PB7) 출력 설정
	uart_init();

	while (1) {
	    if (command_ready) {
		if (my_strcmp(buf, "LED") == 0) {
			PORTB &= ~(1 << PB7); //turn off LED 
			DDRB ^= (1 << DDB7); // LED feature toggle. 
					      // XOR, eq = 0, diff = 1; 
	            }
		    command_ready = 0;
	    }

	    loop_count++;
	    //assuming that 1 loop consume 80 clock cycle, 
	    //then 50000 will be 0.25 second 
	    if (loop_count > 50000) {
                   PINB = (1 << PINB7); // PB7 토글 (On <-> Off)
			loop_count = 0; 
	    }
	}
}

//LED blinking normal, UART interrupt not working 
//
