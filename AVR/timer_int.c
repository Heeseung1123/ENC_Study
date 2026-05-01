#include <avr/io.h>
#include <avr/interrupt.h>

volatile int count = 0;
int patterns[] = {10, 30, 150, 40};
int pattern_index = 0;

ISR(TIMER0_COMPA_vect) {
    count++;
    
    if (count >= patterns[pattern_index]) {
        PINB = (1 << PINB7); // LED 토글
        count = 0;           // 카운트 초기화
        
        pattern_index++;
        if (pattern_index >= 4) {
            pattern_index = 0;
        }
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
    DDRB |= (1 << DDB7); 
    
    setup_timer();
    
    while(1) {
	    //interrupt handler 
    }
}
