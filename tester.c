/**
* A test program for the VCP COM port library written in C
* Tested on Windows 10, Ubuntu 16.04.1
* Authored by William Barber 15/01/17
* Licence: This work is licensed under the MIT License.
*   View this license at https://opensource.org/licenses/MIT
**
* Demonstrates:
*   Opening a serial port
*   Reading data from a serial
*   Writing:
*       A single byte
*       A buffer of characters (i.e. a string)
*       A 16 bit data word
*       A hexadecimal array
*   Closing a serial port
**
* To compile: gcc tester.c -Wall -o tester.exe
**
* To use this program connect the RX & TX lines of the serial port together (local loopback configuration).
* From the command line: tester.exe <COM port number>
* The program prints what is sent each time and then what is received after a short delay.
* The output should look like tester.out
**
* Troubleshooting:
*   If the received lines after don't contain the full data sent increase the delay between the send command and polling the COM port
*   as at 9600 baud a character takes 10.5ms to be transmitted on top of any processing time by the OS.
*
*   If the received value for sending a word has the 2 bytes reversed add the line:
*   #define COMPORT_BIG_ENDIAN
*   before the #include "VCP.h" line as your operating system is using big-endian
**/
#include <stdio.h>
#include <string.h>
#include "VCP.h"

#ifndef _WIN32//windows already contains a Sleep funtion in windows.h
#include <time.h>

void Sleep(int msec){//a sleep function for Unix based OSes that works in milliseconds
	struct timespec wait, left;
	wait.tv_sec = (msec / 1e3);
	wait.tv_nsec = (msec % (int)1e3) * 1000000;
	while(nanosleep(&wait, &left)){//make sure sleep doesn't return early by interruptions
		wait = left;
	}
}
#endif

//Data to send
uint8_t exByte = 'x';
char exStr[] = "Hello World";
uint16_t exWord = 0x45C4;
char exHex[] = "A4a38";

int main(int argc, char* argv[]){
	char buf[128];
	uint8_t hex[10];
	uint8_t len, i;
	if((argc != 2)||(argv[1][0] == '?')){//Operating System dependant help information
		#ifdef _WIN32
		printf("Usage:%s <COM-port>\n", argv[0]);
		printf("e.g.:%s COM3\n", argv[0]);
		#else
		printf("Usage:sudo %s <COM-port>\n", argv[0]);
		printf("e.g.:sudo %s /dev/ttyS0\n", argv[0]);
		#endif
		return 1;
	}
	if(init_COM_port(argv[1], 9600, 0, 1, 0))return -1;//Opens the serial port specified in the command line
	printf("COM port %s opened\n", argv[1]);			//at 9600 baud with no parity and one stop bit

	memset(buf, '\0', 128);
	if(COM_port_send_byte(exByte)==1){//Sending a byte returns 1 on success and a negative error code on failure
		printf("Sent:%c\n", exByte);
	}
	Sleep(20);
	if((len = poll_COM_port(buf, 128))){//Data received by the serial port is moved into the buffer up to the length set
										//the return value is the number of bytes put into the buffer at buf
		printf("Received:");
		for(i = 0; i < len; i++)putchar(buf[i]);//prints only the section of the buffer that has been written
		putchar('\n');
	}

	if(COM_port_send_buffer(exStr, 12)){//Sending a buffer returns the number of bytes sent on success
		printf("Sent:%s\n", exStr);		//and a negative error code on failure
	}
	Sleep(50);
	if((len = poll_COM_port(buf, 128))){
		printf("Received:");
		for(i = 0; i < len; i++)putchar(buf[i]);//prints only the section of the buffer that has been written
		putchar('\n');
	}

	if(COM_port_send_word(exWord)==2){//Sending a byte returns 2 on success and a negative error code on failure
		printf("Sent:%X\n",exWord);
	}
	Sleep(30);
	if(poll_COM_port(buf, 128)){
		printf("Received:%X\n",*((uint16_t*)buf));
		memset(buf, '\0', 128);
	}

	ascii2hex(exHex, hex, &len);//Converts the string at exHex into hexadecimal and stores the output in hex
								//sets len to the number of bytes in the output hex array
	if(COM_port_send_buffer((char*)hex, len)){
		hex2ascii(hex, buf, len);//Converts the byte array of hexadecimal at hex of length len into
									//a null terminated string and stores the output at buf
		printf("Sent:%s (%s)\n", buf,exHex);
	}
	Sleep(50);
	if((len = poll_COM_port(buf, 128))){
		hex2ascii((uint8_t*)buf, (char*)hex, len);
		printf("Received:%s\n",hex);
	}
	close_COM_port();
	return 0;
}
