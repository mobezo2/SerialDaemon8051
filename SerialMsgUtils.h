/*
 * SerialMsgUtils.h
 *
 *  Created on: Sep 9, 2014
 *      Author: mbezold
 */

#ifndef SERIALMSGUTILS_H_
#define SERIALMSGUTILS_H_

#include "typedef.h"

#include <stdio.h>
#include <stddef.h>

#define LINUX

#define MSG_HEADER_LENGTH	13

#define MAX_MSG_SIZE		750

/* Ensure that Encoded bytes are not within the range of
 * observed ascii characters */
#define UINT16_ENCODE  		12336
#define UINT8_ENCODE 		48
/* Packed avoid compiler adding padding in, which is not
 * desireable for our mesage packets */
typedef struct __attribute__((__packed__))PacketHdr{
		uint8_t 	HeaderByte1;
		uint8_t 	HeaderByte2;
		uint8_t 	HeaderByte3;
		uint8_t 	HeaderByte4;
		uint8_t 	HeaderByte5;
		uint8_t 	HeaderByte6;
		uint8_t 	HeaderByte7;
		uint8_t			MsgID;
		uint16_t		MsgLength;
		uint8_t 		MsgFlags;
		uint16_t		SeqCount;
	}PacketHdr;


typedef struct RxMsgInfo{
		uint8_t		MsgID ;
		uint16_t		MsgLength;
		uint8_t 	MsgFlags;
		uint16_t	SeqCount;
	}RxMsgInfo;


#define HEADERBYTE1 'B'
#define HEADERBYTE2 'A'
#define	HEADERBYTE3 'D'
#define	HEADERBYTE4 'C'
#define	HEADERBYTE5 'A'
#define	HEADERBYTE6 'F'
#define	HEADERBYTE7 'E'

	/* Error Codes */
#define PARSE_PKT_NO_HEADER_PRESENT 		0



/* Build Packet Headers, which includes length
 * and message type identifier
 *
 *  INPUTS:
 *  Length - # of bytes in the message
 *  MsgID  - Unique Msg ID for use in routing messages
 *  	by high level software
 *  MsgFlags - Message Flags for describing message,
 *  	by higher level software
 *  SequenceCount - The number of the message, if part
 *  	of a sequence
 *
 *  RETURNS:
 *  Size of header
*/
int32_t
BuildPacketHdr(int32_t Length, uint8_t MsgID, uint8_t MsgFlags, uint16_t SequenceCount, PacketHdr *ReturnPacketHdr );



/* Convert Raw Byte data to array of ASCII characters, representing
 * the HEX values of the raw bytes
 *
 * INPUTS:
 * RawByteIn - Pointer to Input Array of bytes
 * ASCIIHexOut- Pointer to Output array of ASCII characters
 * Length- Bytes in input array
 *
 * RETURNS:
 * Number of bytes in ASCIIHexOut if sucessful
 * Negative if error code if failure   */
int32_t
BytesToASCIIHex(uint8_t *RawByteIn, uint8_t *ASCIIHexOut, int32_t Length );


/* Process Raw Byte Packets, including parsing
 * the packet header to get message info/
 *  INPUTS:
 *  MessageInfo-Pointer to Message Info struct, for storing the
 *  	parsed contens of the packet header
 *  RxMessage - Pointer to the raw bytes
 *
 *
 *  RETURNS:
 *  Memory address of the Actual Data Bytes if sucessful,
 *  negative error code if failure
*/

ARM_char_t
ProcessPacket(RxMsgInfo *MessageInfo, ARM_char_t* RxMessage );

/* Convert ASCII hex Representation of bytes to regular bytes (reverse
 * operations of BytesToASCIIHex */

int32_t
ASCIIHexToBytes( ARM_char_t *ASCIIHexIn, uint8_t *RawByteOut, int32_t Length );

#endif /* SERIALMSGUTILS_H_ */
