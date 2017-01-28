/**
* A test program for the VCP COM port library written in C
* Tested on Windows 10, Ubuntu 16.04.1
* Authored by William Barber 15/01/17
* Licence: This work is licensed under the MIT License.
*   View this license at https://opensource.org/licenses/MIT
**
* The program prints any received data in hexadecimal to stdout and any errors are printed in brackets
* Error information is printed to stderr
* The program terminates after 30 seconds unless errors are encountered where the remaining time is divided by 10
**
* To compile: gcc serTest.c -Wall -o serTest.exe
**
* To use this program connect the RX line of the serial port to the il-matto that is producing output.
* From the command line: serTest.exe <COM port number>
*
* To redirect the received data to a file use: serTest.exe <COM port number> >file.out
* This will leave only stderr being displayed in the terminal
* To redirect the data and errors to separate files use serTest.exe <COM port number> >file.out 2>error.out
**
* NOTE: Currently if the serial port is closed externally polling the port returns 0 instead of an error code
**/

#include <stdio.h>
#include <ctype.h>
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

int main(int argc, char* argv[]){
	int i = 300,j,len;
	char msg_recv[1000];
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
	init_COM_port(argv[1], 9600, 0, 1, 0);//Opens the serial port specified in the command line
											//at 9600 baud with no parity and one stop bit
	while(i-- > 0){//Loops for 30 seconds checking every 0.1 seconds
		len = poll_COM_port(msg_recv, 1000);
		if(len < 0){
			printf("(%d)",len);//print out error numbers in brackets
			#ifdef _WIN32 //Operating system dependant generation of error information to stderr
			char buffer[1024];
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0,(0-len),0,buffer,1024,NULL);
			fprintf(stderr,"%s",buffer);
			#else
			perror(NULL);
			#endif
			i -= 10;//reduce the number of printouts when errors are produced
		}else{
			for(j=0;j<len;j++){
				printf("%02X ",msg_recv[j]);//Print all data received as hex
			}
		}
		Sleep(100);
	}
	close_COM_port();
	return 1;
}
