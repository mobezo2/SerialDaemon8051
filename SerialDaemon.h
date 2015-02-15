/*
 * SerialDaemon.h
 *
 *  Created on: Sep 23, 2014
 *      Author: mbezold
 */

#ifndef SERIALDAEMON_H_
#define SERIALDAEMON_H_

#define SERIAL_DAEMON_SEM "/SerialDaemonSem"

#define SERIAL_RX_LOG_FILENAME "SerialRXLog.txt"
#define SERIAL_TX_LOG_FILENAME "SerialTXLog.txt"

#define DAEMON8051_LOG_NAME "SerialDaemon8051"


#define SERIAL_FILEPATH "/dev/ttyO4"
//#define FILE_OUT_BUFF_SIZE_DAEMON	60
#define MAX_READ_BYTES 		255
#define MAX_RX_BUFF_SIZE 	1020

/* Error Return Codes */
#define SEM_OPEN_FAIL -1
#define SERIAL_TX_WRITE_FAIL -2
#define SERIAL_TX_ZERO_BYTES -3
#define DAEMON_FAIL 		 -100


/* Error Return Codes for SerialConfigure() */
#define 	OPEN_FAIL		-4
#define 	TCGETATTR_FAIL	-5
#define		TCSETATTR_FAIL	-6
#define		FCNTL_MODE		-7
#define     BAUDRATE_FAIL   -8
#define 	SETOWN_FAIL		-9

#define SEM_OPEN_FAIL -1
#define SERIAL_TX_WRITE_FAIL -2
#define SERIAL_TX_ZERO_BYTES -3
#define DAEMON_FAIL 		 -100


/* Baud Rates of Serial Ports
 * Valid Baud Rates = B300, B2400, B9600, B38400
 * Don't forget the B in front!!! */
#define 	TTYBAUDRATE		B9600

#endif /* SERIALDAEMON_H_ */
