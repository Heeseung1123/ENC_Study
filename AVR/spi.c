#define F_CPU 16000000UL

#include <util/delay.h>
#include <avr/io.h>


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
	SPI_MasterTransmit(0x40 | address); //write Opcode 010
	SPI_MasterTransmit(data); //8 bit data 
	PORT_SPI |= (1 << DD_CS);
}


void ENC_WritePhy(uint8_t address, uint16_t data) {
	ENC_WriteReg(0x1f, 0x02);	//change to bank2
	ENC_WriteReg(0x14, address);      // MIREGADR에 PHY 레지스터 주소 기록
	ENC_WriteReg(0x16, data & 0xFF);  // MIWR 하위 바이트 기록
	ENC_WriteReg(0x17, data >> 8);    // MIWR 상위 바이트 기록
	_delay_us(200);                   // PHY 처리 대기
}


int main(void) {	
    SPI_MasterInit();

    while(1) {
        ENC_WritePhy(0x14, 0x3880);
        _delay_ms(500);

        ENC_WritePhy(0x14, 0x3990);
        _delay_ms(500);
    }
}

