#include <avr/io.h>

#define F_CPU 16000000UL
#define BAUD 9600
#define UBRR_VAL 103

void uart_init(void) {
    // main.c:10-11
    UBRR0H = (unsigned char)(UBRR_VAL >> 8);
    UBRR0L = (unsigned char)UBRR_VAL;

    // main.c:16: TXEN0(송신) 및 RXEN0(수신) 활성화
    UCSR0B = (1 << TXEN0) | (1 << RXEN0); 

    // main.c:19: 8-bit 데이터 모드 설정
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

int main(void) {
	unsigned char buf[10];
	int i = 0;
	unsigned char tmp;

    uart_init();
    DDRB |= (1 << DDB7); // LED3(PB7) 출력 설정


    while (1) {
        tmp = uart_receive();

        if (tmp == '\r' || tmp == '\n') { // 엔터 입력 시
            buf[i] = '\0'; // 문자열 종료
            uart_transmit('\r');
            uart_transmit('\n');

		if (my_strcmp(buf, "LED") == 0) {
                    PINB = (1 << PINB7); // PB7 토글 (On <-> Off)
	            }

            i = 0; // 버퍼 인덱스 초기화
            uart_transmit('#');
	    uart_transmit(' ');
        }
        else { // 일반 문자 입력 시
                buf[i++] = tmp;
                uart_transmit(tmp); 
            }
        }
}


