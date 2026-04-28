default: 
	avr-gcc -Wall -Os -mmcu=atmega2560 -c main.c -o main.o
	avr-gcc -mmcu=atmega2560 main.o -o main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
