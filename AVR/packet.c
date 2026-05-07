#include "include/enc.h"

void ENC_ReadBuffer(uint16_t address, uint16_t length, uint8_t *buf) {

	ENC_WriteReg(0x1f, 0x00);	//change to bank0 for ERDPT
	ENC_WriteReg(0x00, (uint8_t)(address & 0xFF));	//set ERDPTL
	ENC_WriteReg(0x01, (uint8_t)(address >> 8));	//set ERDPTH  
	


	PORT_SPI &= ~(1 << DD_CS);

	SPI_MasterTransmit(0x3A);	//set RBM instruction
					
	for(uint16_t i = 0; i < length; i++) { //read "length" byte 
		buf[i] = SPI_MasterTransmit(0x00);
		_delay_ms(10);
	}
	

	PORT_SPI |= (1 << DD_CS);
}

void ENC_WriteBuffer(uint16_t address, uint16_t length, uint8_t *buf) {
    ENC_WriteReg(0x1F, 0x00);

    ENC_WriteReg(0x02, (uint8_t)(address & 0xFF)); // set EWRPTL
    ENC_WriteReg(0x03, (uint8_t)(address >> 8));   // set EWRPTH

    PORT_SPI &= ~(1 << DD_CS);

    
    SPI_MasterTransmit(0x7A);

    //due to voltage drop issue after 3 byte, we manually turn on/off the ss
    //pin to recharge voltage.
   for(uint16_t i = 0; i < length; i++) {

	ENC_WriteReg(0x1F, 0x00);
        ENC_WriteReg(0x02, (uint8_t)((address + i) & 0xFF));
        ENC_WriteReg(0x03, (uint8_t)((address + i) >> 8));

        PORT_SPI &= ~(1 << DD_CS);
        SPI_MasterTransmit(0x7A); // SRAM Write Opcode 
        SPI_MasterTransmit(buf[i]);
        PORT_SPI |= (1 << DD_CS);

    }


    PORT_SPI |= (1 << DD_CS);
}

// 문자열 출력용
void uart_transmit_str(const char* s) {
    while (*s) uart_transmit(*s++);
}

// 8비트 값을 16진수 문자로 출력 (예: 0xAB -> "AB")
void uart_transmit_hex(uint8_t val) {
    char *hex = "0123456789ABCDEF";
    uart_transmit(hex[(val >> 4) & 0x0F]);
    uart_transmit(hex[val & 0x0F]);
}



void test_enc_sram(void) {
    uint8_t test_data_out[10] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint8_t test_data_in[10] = {0};
    uint16_t test_addr = 0x1000; // 테스트용 임의 주소 (보통 TX Buffer 영역)
    int success = 1;

    uart_transmit_str("\r\n--- ENC28J60 SRAM Test Start ---\r\n");

    // 1. 데이터 쓰기
   ENC_WriteBuffer(test_addr, 10, test_data_out);
    uart_transmit_str("Writing data to SRAM...\r\n");

    // 2. 데이터 읽기
    ENC_ReadBuffer(test_addr, 10, test_data_in);
    uart_transmit_str("Reading data back from SRAM...\r\n");

    // 3. 비교 및 출력
    for (int j = 0; j < 10; j++) {
        // 읽어온 값 확인을 위해 UART 출력 (16진수)
        uart_transmit_hex(test_data_in[j]);
        uart_transmit(' ');

        if (test_data_out[j] != test_data_in[j]) {
            success = 0;
        }
    }

    if (success) {
        uart_transmit_str("\r\n[SUCCESS] Write/Read Match!\r\n");
    } else {
        uart_transmit_str("\r\n[FAIL] Data Mismatch!\r\n");
    }
}


int main(void) {
    // 하드웨어 초기화
     //
    uart_init();
    SPI_MasterInit();
    
	ENC_SoftReset();
    // ENC28J60 기본 초기화 (최소한 뱅크 접근이 가능한 상태여야 함)
	 ENC_WriteReg(0x1F, 0x00); // 명시적으로 Bank0 선택
 
    // 테스트 코드 예시
    uart_transmit_str("ENC28J60 Static Test Start...\r\n");

    while (1) {
	    ENC_WriteReg(0x1f,0x03);
	    
	    uint8_t test = ENC_ReadReg(0x12);
	    uart_transmit_str("Revision number: ");
	    uart_transmit_hex(test);
	    uart_transmit('\r');
	    uart_transmit('\n');

        // 깃발(command_ready) 기다리지 말고
        // 그냥 2초마다 한 번씩 자동으로 테스트 실행
      test_enc_sram();

        _delay_ms(1000);
    }

    return 0;
}

