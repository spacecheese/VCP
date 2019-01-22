/**
* A COM port library for Windows and Posix based operating systems written in C
* Tested on Windows 10, Ubuntu 16.04.1 and OS X
* Authored by William Barber 13/01/17
* Licence: This work is licensed under the MIT License. 
* 	View this license at https://opensource.org/licenses/MIT
* Implements:
* + Opening COM ports with customisable baudrate, parity & stop bits
* + Reading and writing to the COM port with non-blocking read operation
* + Converting between hexadecimal and ascii
* 
* ##Windows##
* Information for the functions required to implement COM port functionality comes from https://msdn.microsoft.com/en-us/library/ff802693.aspx
* For in depth information about all the COM port specific functions look at https://msdn.microsoft.com/en-us/library/windows/desktop/aa363194(v=vs.85).aspx
* 
* ##Posix##
* Information for the functions and libraries required to implement COM port functionality comes from http://www.cmrr.umn.edu/~strupp/serial.html
**/
#pragma once
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>

static HANDLE hComm = NULL;
static DCB DCBinitial;
static COMMTIMEOUTS TIMEOUTinitial;
#else
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */

static int hComm = 0;//The file descriptor of the COM port
static struct termios termiosInitial;
#endif

//Open a COM port of the supplied name and configures baud-rate, frame size, parity & stop bits settings and sets time-out values
int init_COM_port(char* portName, int baudRate, uint8_t parityBit, uint8_t stopBits, uint8_t mode);
//Tests if the opened COM port has received data, returns immediately if no data has been received else returns received data up to a size of length
int poll_COM_port(char* buffer, uint16_t length);
//Sends a buffer of data of length, returns the number of bytes transmitted or a negative error code
int COM_port_send_buffer(char* buffer, uint8_t length);
//Sends a byte of data , returns 1 if the byte is transmitted or a negative error code
int COM_port_send_byte(uint8_t data);
//Sends a word (16-bit) of data , returns 2 if the word is transmitted or a negative error code
int COM_port_send_word(uint16_t data);
//Closes the COM port, resetting the port settings and time-outs to default
void close_COM_port();
//Takes an ASCII string(null terminated) of hexadecimal characters and converts their values into a byte array, returning the length of the produced array.
//Note: If the length of the string of characters is odd a '0' character is appended in the byte array to fill the byte
int ascii2hex(char* str, uint8_t* data, uint8_t* len);
//Takes a byte array and converts the byte values in to an ASCII string of hexadecimal characters, returning the length of the produced string.
int hex2ascii(uint8_t* data, char* str, uint8_t len);

int init_COM_port(char* portName, int baudRate, uint8_t parityBit, uint8_t stopBits, uint8_t mode){
	/*Options:
	Parity:
		0 - No parity
		1 - Odd parity
		2 - Even parity
	Stop bits:
		1 - One stop bit
		2 - Two stop bits
	Mode: (not implemented)
		0 - Non-overlapped, returns immediately
		1 - Non-overlapped, moderate time-out
	*/
	
	//Operating systems treat all external data sources as a file so COM ports are opened by opening a file of the COM port's name
	//Create the COM port handle
#ifdef _WIN32
	int BaudR[] ={CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400, CBR_4800, CBR_9600, CBR_14400,
					CBR_19200, CBR_38400, CBR_57600, CBR_115200, CBR_128000, CBR_256000};//Baud-rates supported by Windows
	int BaudRno[] ={110, 300, 600, 1200, 2400, 4800, 9600, 14400,
					19200, 38400, 57600, 115200, 128000, 256000};
	//Create the COM port handle
	//CreateFile() syntax from https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
	hComm = CreateFile( portName,		//COM port name
		GENERIC_READ | GENERIC_WRITE,	//Opens port for send & receive
		0,								//COM ports cannot be opened for sharing
		NULL,							//COM ports use the default security descriptor
		OPEN_EXISTING,					//If the COM port doesn't exists the function fails
		///FILE_FLAG_OVERLAPPED,		//
		FILE_ATTRIBUTE_NORMAL,			//Starts COM port in non-overlappped mode (blocking functions)
		0);								//Templates are unused when opening COM ports
	if(hComm == INVALID_HANDLE_VALUE){//Failed to open COM port
		fprintf(stderr,"Error opening COM port:%s, Error Code:%lu\n", portName, GetLastError());
		return -1;
	}
	
	//DCB is the structure where Windows stores all the COM port settings
	//DCB structure information from https://msdn.microsoft.com/en-us/library/windows/desktop/aa363214(v=vs.85).aspx
	//Set-up DCB settings
	DCB DCBsettings;
	if (!GetCommState(hComm, &DCBinitial)){	//Get current DCB
		fprintf(stderr,"Error retrieving DCB settings for COM port, Error Code:%lu\n", GetLastError());//Failed to retrieve current COM port settings
		CloseHandle(hComm);
		return -1;
	}
	DCBsettings = DCBinitial;//Create settings from the initial values
	//Update the values
	DCBsettings.ByteSize = 8;//Sets the size of each transmission to 8 data bits
	uint8_t i;
	for(i=0; i<14; i++) {//Set baud-rate if the value is a supported baud-rate
		if(baudRate == BaudRno[i]){
			DCBsettings.BaudRate = BaudR[i];
			break;
		}
	}
	
	DCBsettings.Parity = ((parityBit < 3)?parityBit:0);//Sets the parity type according to the DCB's format
	DCBsettings.StopBits = ((stopBits == 2)?TWOSTOPBITS:ONESTOPBIT);//Sets the number of stop bits according to the DCB's format
	if (!SetCommState(hComm, &DCBsettings)){//Set updated DCB
		fprintf(stderr,"Error setting DCB settings for COM port, Error Code:%lu\n", GetLastError());//Failed to set updated COM port settings
		CloseHandle(hComm);
		return -1;
	}
	
	//Windows has a structure for defining different elements of the COM ports time-out(the time before the function returns unless all the data specified has been sent or received)
	//Information about the elements is from https://msdn.microsoft.com/en-us/library/windows/desktop/aa363190(v=vs.85).aspx
	//Set-up time-outs
	COMMTIMEOUTS timeouts;
	if(!GetCommTimeouts(hComm, &TIMEOUTinitial)){//Get current time-outs
		fprintf(stderr,"Error retrieving time-out settings for COM port, Error Code:%lu\n", GetLastError());//Failed to retrieve current COM port time-outs
		CloseHandle(hComm);
		return -1;
	}
	timeouts = TIMEOUTinitial;//Create time-outs from the initial values
	//Update the values
	//setting time-outs to 0 disables them
	//When ReadInterval is MAXDWORD and Multiplier & Constant are 0, read will return immediately
	timeouts.ReadIntervalTimeout = MAXDWORD;	//Maximum time in milliseconds allowed between bytes arriving
	timeouts.ReadTotalTimeoutMultiplier = 0;	//Time allowed for each byte expected to be received
	timeouts.ReadTotalTimeoutConstant = 0;		//Time allowed for overhead on the transmission
	timeouts.WriteTotalTimeoutMultiplier = 0;	//Time allowed for each byte to be transmitted
	timeouts.WriteTotalTimeoutConstant = 0;		//Time allowed for overhead on the transmission
	if(!SetCommTimeouts(hComm, &timeouts)){//Set updated time-outs
		fprintf(stderr,"Error setting time-out settings for COM port, Error Code:%lu\n", GetLastError());//Failed to set updated COM port time-outs
		CloseHandle(hComm);
		return -1;
	}
#else
	int BaudR[] ={B0, B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800,
				B2400, B4800, B9600, B19200, B38400, B57600, B115200, B230400};//Baud-rates supported by Posix
	int BaudRno[] ={0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800,
				2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400};
	//open() syntax from http://www.gnu.org/software/libc/manual/html_mono/libc.html#Low_002dLevel-I_002fO
	//flag definitions from http://www.gnu.org/software/libc/manual/html_mono/libc.html#File-Status-Flags
	hComm = open( portName,		//COM port name
		O_RDWR					//Opens port for send & receive
		| O_NOCTTY				//Don't make the port the controlling terminal, i.e. redirect stdin & stdout to it.
		| O_NONBLOCK);			//Open the port in non-blocking so read returns if there is no data
	if(hComm < 0){//Failed to open COM port
		fprintf(stderr,"Error opening COM port:%s, Error Code:%d\n", portName, errno);
		return -1;
	}

	//termios is the structure where Posix stores all the terminal interface settings
	//termios structure information from http://www.gnu.org/software/libc/manual/html_mono/libc.html#Terminal-Modes
	//Set-up termios settings
	struct termios termiosSettings;
	if (tcgetattr(hComm, &termiosInitial)){ //Get current termios values
		fprintf(stderr,"Error retrieving termios settings for COM port, Error Code:%d\n", errno);//Failed to retrieve current COM port settings
		close(hComm);
		return -1;
	}
	termiosSettings = termiosInitial;//Create settings from the initial values
	//Set required fields
	termiosSettings.c_iflag |= INPCK | IGNPAR;//Set the terminal to remove bytes with parity error
	termiosSettings.c_iflag &= ~(PARMRK | ISTRIP | IGNCR | INLCR);//and leave other bytes unaffected
	termiosSettings.c_iflag &= ~(IXON | IXOFF);//Disable software flow control

	termiosSettings.c_oflag &= ~OPOST;//Disable any processing on data before transmitting

	termiosSettings.c_cflag |= CLOCAL | CREAD;//Disable modem hangup settings
	termiosSettings.c_cflag &= ~HUPCL;
	termiosSettings.c_cflag &= ~(CSTOPB | PARENB | CSIZE);//Clear stop, size & parity values ready to set

	termiosSettings.c_lflag &= ~ICANON;//Disable canonical input so received data is not held pending a new-line character
	termiosSettings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHOKE | ECHONL);//Disable COM port echo received data back to the COM port

	//Update the values
	termiosSettings.c_cflag |= CS8;//Sets the size of each transmission to 8 data bits
	uint8_t i;
	for(i=0; i<(sizeof(BaudR)/sizeof(int)); i++) {//Set baud-rate if the value is a supported baud-rate
		if(baudRate == BaudRno[i]){
			cfsetospeed(&termiosSettings, BaudR[i]);//Set COM port speed
			cfsetispeed(&termiosSettings, BaudR[i]);
			break;
		}
	}
	termiosSettings.c_cflag |= ((parityBit==0)?0:PARENB);//Sets the parity type according to the structure's format
	termiosSettings.c_cflag |= ((parityBit==1)?PARODD:0);
	termiosSettings.c_cflag |= ((stopBits==2)?CSTOPB:0);//Sets the number of stop bits according to the structure's format

	termiosSettings.c_cc[VMIN] = 0;//Disable blocking on read
	termiosSettings.c_cc[VTIME] = 0;

	if (tcsetattr(hComm, TCSADRAIN, &termiosSettings)){//Set updated DCB
		fprintf(stderr,"Error setting termios settings for COM port, Error Code:%d\n", errno);//Failed to set updated COM port settings
		close(hComm);
		return -1;
	}
#endif
	return 0;
}

int poll_COM_port(char* buffer, uint16_t length){
#ifdef _WIN32
	//ReadFile() syntax from https://msdn.microsoft.com/en-us/library/windows/desktop/aa365467(v=vs.85).aspx
	DWORD len = 0;
	if(ReadFile(hComm,	//The COM port handle
	buffer,				//The buffer for the data
	length,				//The maximum data to read
	&len,				//The variable to store the number of bytes read
	NULL)){				//Overlapped operation is not used
		return (int)len;
	}else{
		return 0-GetLastError();//if reading the port failed a negative length is returned corresponding to the windows error code
	}
#else
	//read() syntax from http://www.gnu.org/software/libc/manual/html_mono/libc.html#I_002fO-Primitives
	int len = read(hComm,	//The COM port handle
	buffer,					//The buffer for the data
	length);				//The maximum data to read
	if(len >= 0){
		return (int)len;
	}else if(errno == EWOULDBLOCK){
		return 0;//no data retrieved
	}else{
		return 0-errno;//if reading the port failed a negative length is returned corresponding to the windows error code
	}
#endif
}

int COM_port_send_buffer(char* buffer, uint8_t length){
#ifdef _WIN32
	//WriteFile() syntax from https://msdn.microsoft.com/en-us/library/windows/desktop/aa365747(v=vs.85).aspx
	DWORD len = 0;
	if(WriteFile(hComm,	//The COM port handle
	buffer,				//The buffer for the data
	length,				//The length of data to transmit
	&len,				//The variable to store the number of bytes transmitted
	NULL)){				//Overlapped operation is not used
		return (int)len;
	}else{
		return 0-GetLastError();//if writing to the port failed a negative length is returned corresponding to the windows error code
	}
#else
	//write() syntax from http://www.gnu.org/software/libc/manual/html_mono/libc.html#I_002fO-Primitives
	int len = write(hComm,	//The COM port handle
	buffer,						//The buffer for the data
	length);					//The length of data to transmit
	if(len >= 0){
		return (int)len;
	}else{
		return 0-errno;//if writing to the port failed a negative length is returned corresponding to the windows error code
	}
#endif
}

int COM_port_send_byte(uint8_t data){
#ifdef _WIN32
	DWORD len = 0;
	if(WriteFile(hComm, &data, 1, &len, NULL)){
		return (int)len;
	}else{
		return 0-GetLastError();//if writing to the port failed a negative length is returned corresponding to the windows error code
	}
#else
	int len = write(hComm, &data, 1);
	if(len >= 0){
		return (int)len;
	}else{
		return 0-errno;//if writing to the port failed a negative length is returned corresponding to the windows error code
	}
#endif
}

int COM_port_send_word(uint16_t data){
	uint8_t buffer[2] = 
	#ifdef COMPORT_BIG_ENDIAN
	{(uint8_t)(data>>8),(uint8_t)(data & 0xFF)};//In big endian the most significant byte is sent first
	#else
	{(uint8_t)(data & 0xFF),(uint8_t)(data>>8)};//In little endian the least significant byte is sent first
	#endif
#ifdef _WIN32
	DWORD len = 0;
	if(WriteFile(hComm, &buffer, 2, &len, NULL)){
		return (int)len;
	}else{
		return 0-GetLastError();//if writing to the port failed a negative length is returned corresponding to the windows error code
	}
#else
	int len = write(hComm, &buffer, 2);
	if(len >= 0){
		return (int)len;
	}else{
		return 0-errno;//if writing to the port failed a negative length is returned corresponding to the windows error code
	}
#endif
}

void close_COM_port() {
#ifdef _WIN32
	SetCommState(hComm, &DCBinitial);//Reset the DCB to it's initial state
	SetCommTimeouts(hComm, &TIMEOUTinitial);//Reset the time-outs to their initial states
	CloseHandle(hComm);//Close the port's file interface
	hComm = INVALID_HANDLE_VALUE;
#else
	tcsetattr(hComm, TCSAFLUSH, &termiosInitial);//Reset termios to it's initial state
	close(hComm);//Close the port's file interface
	hComm = -1;
#endif
}

int ascii2hex(char* str, uint8_t* data, uint8_t* len){
	//Reverse engineered from https://github.com/TBTerra/pictor, pictor.c, pictorDrawX() which is used in hex2ascii()
	//An additional clause is added to handle a-f as well as A-F, all non-hex characters are ignored
	//Bytes are in big endian format, where the most significant byte is sent first
	uint8_t i=0, j=0, mid=0;
	while(str[i]){
		if((str[i] >= 48) && (str[i] <= 57)){//Character is 0-9
			if(mid){//Byte packing
				data[j++] |= (str[i] - 48);//lower half of the byte
				mid = 0;
			}else{
				data[j] = ((str[i] - 48)<<4);//upper half of the byte
				mid = 1;
			}
		}else if((str[i] >= 65) && (str[i] <= 70)){//Character is A-F
			if(mid){//Byte packing
				data[j++] |= (str[i] - 55);//lower half of the byte
				mid = 0;
			}else{
				data[j] = ((str[i] - 55)<<4);//upper half of the byte
				mid = 1;
			}
		}else if((str[i] >= 97) && (str[i] <= 102)){//Character is a-f
			if(mid){//Byte packing
				data[j++] |= (str[i] - 87);//lower half of the byte
				mid = 0;
			}else{
				data[j] = ((str[i] - 87)<<4);//upper half of the byte
				mid = 1;
			}
		}
		i++;
	}
	if(mid)j++;
	*len = j;//returns the array length to a pointer as well as from the function
	return j;
}

int hex2ascii(uint8_t* data, char* str, uint8_t len){
	//As 0-9 and A-F are contiguous sets of ASCII codes only loose remapping is required
	//Code taken from https://github.com/TBTerra/pictor, pictor.c, pictorDrawX()
	uint8_t i=0, j=0, x, y;
	while(len--){
		x = (data[i]&0xF0)>>4;//Break byte in half to map to each character
		y = data[i]&0x0F;
		i++;
		str[j++]=(x>=10)?(x+55):(x+48);//convert to characters
		str[j++]=(y>=10)?(y+55):(y+48);//0-9 have ASCII codes 48-57 and A-F have ASCII codes 65-70
	}
	str[j] = '\0';
	return j;
}