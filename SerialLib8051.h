/*
 * SerialLib8051.h
 *
 *  Created on: Sep 8, 2014
 *      Author: mbezold
 */

#include "typedef.h"
#include "SerialMsgUtils.h"
#include <syslog.h>


#ifndef SERIALLIB8051_H_
#define SERIALLIB8051_H_

/* constant char replacement, must follow a path name convention,
 * will fail unless executing process has permissions for this
 * path!!! */
#define SERIAL_TX_QUEUE "/TxMq"
#define SERIAL_RX_QUEUE "/RxMqSupervisor"


/* test files */
#define TEST_OUPUT_FILENAME "SerialOutputFile.txt"
#define TEST_INPUT_FILENAME "SerialInputFile.txt"


#define MAX_TX_BYTES	360

/* Message Queue settings */
#define MAX_MSG_CNT 		100
#define MAX_MSG_SIZE		750


/* codes to keep track of errors */


#define FILE_OUT_BUFF_SIZE	360

#define SERIAL_FILEPATH "/dev/ttyO4"

#define MAX_READ_BYTES 		255
#define MAX_RX_BUFF_SIZE 	1020

int32_t Serial8051Open(const char *);
int32_t Serial8051Send(uint8_t *TxBuffer, int32_t Length, uint8_t MsgID, uint16_t SequenceCount, uint8_t MsgFlags, uint32_t Priority);
int32_t Serial8051Receive(uint8_t *RxBuffer, RxMsgInfo * CurrentMsgInfo );


/* Error Return Codes */
#define OVERSIZE_MSG_ERROR 					-1
#define SERIAL_RECEIVE_OPEN_FAILURE 		-1
#define SERIAL_RECEIVE_MESSAGE_ATTR_FAIL 	-2
#define SERIAL_RECEIVE_BUFF_ALLOCATE_FAIL 	-3
#define SERIAL_RECEIVE_MSG_READ_FAIL 		-4
#define SERIAL_RECEIVE_NO_HEADER_FAIL		-5
#define SEM_OPEN_FAILURE					-1
#define SEM_GET_VALUE_FAIL					-2
#define SEM_QUEUE_FAIL						-3
#define SEM_CLOSE_FAIL						-4
#define MSG_QUEUE_OPEN_FAIL 				-1
#define MSG_SEND_FAIL						-2
#define MSG_CLOSE_FAIL						-3


#endif /* SERIALTESTJIG_H_ */
