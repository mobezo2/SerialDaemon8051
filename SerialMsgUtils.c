#include "typedef.h"
#include "SerialMsgUtils.h"
#include <stdio.h>
#include <stddef.h>


#ifdef LINUX
	#include "error_functions.h"
	#include <syslog.h>
#endif //LINUX

#define DEBUG_LEVEL 16

/* Enable display of message length in process packet below */
#define DISPLAY_MSG_LENGTH 0

/*
 * SerialMsgUtils.h
 *
 *  Created on: Sep 8, 2014
 *      Author: mbezold
 */
/* Build Packet Headers, which includes length
 * and message type identifier
 *
 *  INPUTS:
 *  Length - # of bytes in the message
 *  MsgID  - Unique Msg ID for use in routing messages
 *  by high level software
 *
 *  RETURNS:
 *  Length of the Header if sucessful,
 *  negative error code if failure
*/

int32_t
BuildPacketHdr(int32_t Length, uint8_t MsgID, uint8_t MsgFlags, uint16_t SequenceCount, PacketHdr *ReturnPacketHdr ){

	/* Declare the packet header initial bytes */
	ReturnPacketHdr-> HeaderByte1 = HEADERBYTE1;
	ReturnPacketHdr-> HeaderByte2 = HEADERBYTE2;
	ReturnPacketHdr-> HeaderByte3 = HEADERBYTE3;
	ReturnPacketHdr-> HeaderByte4 = HEADERBYTE4;
	ReturnPacketHdr-> HeaderByte5 = HEADERBYTE5;
	ReturnPacketHdr-> HeaderByte6 = HEADERBYTE6;
	ReturnPacketHdr-> HeaderByte7 = HEADERBYTE7;

	/* UINT8_ENCODE adds 48 to message ID to prevent Message ID in range of ASCII charcters like New Line,
	 * which could confuse the serial Hardware */
	ReturnPacketHdr->MsgID=MsgID + UINT8_ENCODE;
	/* Length is in bytes (after raw bytes get converted to ASCII (
	 * HEADER_ENCODE==12288, ensures that upper and lower bytes are not
	 * any reserved bytes like New Line, +1 is for a new line byte */
	ReturnPacketHdr->MsgLength=(Length*2) + MSG_HEADER_LENGTH + 1 + UINT16_ENCODE;
	/* Add 48 to Message Flags to prevent MessageFlags in range of ASCII charcters like New Line,
	 * which could confuse the serial Hardware */
	ReturnPacketHdr->MsgFlags=MsgFlags + UINT8_ENCODE;
	/* Add 48 to Sequence Count to prevent data in range of ASCII charcters like New Line,
	 * which could confuse the serial Hardware */
	ReturnPacketHdr->SeqCount=SequenceCount + UINT8_ENCODE;

	return (sizeof(PacketHdr));
}

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
ProcessPacket(RxMsgInfo *MessageInfo, ARM_char_t* RxMessage ){

#if DEBUG_LEVEL > 15
	int i=0;
#endif

	/* Check for our initial header bytes */
	if(RxMessage[0] == HEADERBYTE1 &&
	   RxMessage[1] == HEADERBYTE2 &&
	   RxMessage[2] == HEADERBYTE3 &&
	   RxMessage[3] == HEADERBYTE4 &&
	   RxMessage[4] == HEADERBYTE5 &&
	   RxMessage[5] == HEADERBYTE6 &&
	   RxMessage[6] == HEADERBYTE7 ){

	/* Remove encoding artifacts from the Build packet header step */
	MessageInfo->MsgID=RxMessage[7]- UINT8_ENCODE;;
	/* Combine two bytes into unsigned 16 bit integer, -1 accounts for new line character */
	MessageInfo->MsgLength=(( ( ( ( (uint16_t )RxMessage[9] ) << 8 )| (uint16_t )RxMessage[8] )) - MSG_HEADER_LENGTH - UINT16_ENCODE)/2;

#if DISPLAY_MSG_LENGTH == 1
	char UsrMsg[75];
	openlog("SerialDaemon8051 Process Packet", LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER );
	sprintf(UsrMsg, " The message length is %u", MessageInfo->MsgLength);
	syslog(LOG_INFO, "%s", UsrMsg);
	closelog();
#endif

	/* Prevent buffer overflows that could result from the MsgLength being wrong (due to Endian stuff)  */
	if( MessageInfo->MsgLength > MAX_MSG_SIZE )
	{
		MessageInfo->MsgLength=(( ( ( ( (uint16_t )RxMessage[8] ) << 8 )| (uint16_t )RxMessage[9] )) - MSG_HEADER_LENGTH - UINT16_ENCODE)/2;
	}

	#if DEBUG_LEVEL > 15
		printf("\n Header: ");
					for(i=0; i<( MSG_HEADER_LENGTH ); i++){
								printf(" %x",RxMessage[i]);
						}
					printf("\n");
	#endif //DEBUG_LEVEL

	MessageInfo->MsgFlags=RxMessage[10]- UINT8_ENCODE;;
	/* Make sure you are using the right Endian!!!!! Otherwise you might get a too large or too
	 * small number here */
	MessageInfo->SeqCount=  ( ( ( (uint16_t )RxMessage[13] ) << 8 ) | (uint16_t )RxMessage[11] ) -UINT16_ENCODE;

	/* Address in Buffer where the data bytes start */
	ARM_char_t DataIndex = 13;

	return ( DataIndex );
	}
	else{
	#if DEBUG_LEVEL > 15
	#ifdef LINUX
		printf("ProcessPacketHdr: No header present\n");
	#endif //LINUX
	#endif //DEBUG_LEVEL . 15

		return PARSE_PKT_NO_HEADER_PRESENT;
	}
}



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
BytesToASCIIHex(uint8_t *RawByteIn, uint8_t *ASCIIHexOut, int32_t Length ){

	uint16_t i=0, OutIndex=0;
	uint8_t UpperNibble = 0, LowerNibble = 0;
	int32_t ASCIICnt = Length*2;

	/* Initialize storage for the ASCII hex bytes, multiply by two because
	 * each nibble of the Raw Bytes is represnted by an ASCII character,
	 * encoding the hex values */
	//ASCIIHexOut = (uint8_t*)malloc((uint16_t)ASCIICnt);

	for(i = 0; i < Length; i++){

		/* Bit masking and left
		 * shift gives us Upper Nibble
		 * with decimal value between 0 and 15,
		 * representable by single hex value  */
		UpperNibble = RawByteIn[i] & 0xF0;
		UpperNibble >>= 4;

		/* Convert to decimal representation of ASCII
		 * character representing the hex value by adding
		 * a number to convert to the decimal represnetation
		 * of corresponding hex value */
		/* i.e. Decimal 9 = 0x0A and ASCII 'A'= Decimal 48 */
		/* Decimal 10 = 0x0A and ASCII 'A'= Decimal 65 */
		if(UpperNibble >= 0x0A)
			UpperNibble+=55;

		else
			UpperNibble+=48;

		/* Repeat Upper nibble ASCII conversion for the lower
		 * nibble */
		LowerNibble = RawByteIn[i] & 0x00FF;

		if(LowerNibble >= 0x0A)
			LowerNibble+=55;

		else
			LowerNibble+=48;

		/* Place in output buffer */
		ASCIIHexOut[OutIndex]	= UpperNibble;
		ASCIIHexOut[OutIndex+1] = LowerNibble;

		/* Move to the next index for upper and lower nibbles */
		OutIndex+=2;
	}


	return ASCIICnt;

}

/* Convert ASCII characters to array of Raw Bytes, representing
 * the hex Values of Characters (ASCII A = 0xa)
 *
 * INPUTS:
 * ASCIIHexIn - Pointer to Input Array of ASCII
 * ASCIIHexOut- Pointer to Output array of Bytes
 * Length- Bytes in input array
 * DataPointer - Pointer to the start of the data in the received hex byte buffer
 *
 * RETURNS:
 * Number of bytes in RawByteOut if sucessful
 * Negative if error code if failure   */
int32_t
ASCIIHexToBytes( ARM_char_t *ASCIIHexIn, uint8_t *RawByteOut, int32_t Length ){

	uint16_t i=0, OutIndex=0;
	uint8_t UpperNibble = 0, LowerNibble = 0;

	/*divide by two because
     * each  ASCII character represnents encodes the hex value of
		 * the upper and lower nibbles of the Raw Bytes */
	int32_t RawByteCnt = Length/2;
	i = 0;
	while(i < Length ){

		/* Bit masking and left
		 * shift gives us Upper Nibble
		 * with decimal value between 0 and 15,
		 * representable by single hex value  */
		UpperNibble = ASCIIHexIn[i];
		LowerNibble = ASCIIHexIn[i+1];

		/* Convert to decimal representation of Raw bytes
		 * represented by ASCII characters  */
		/* i.e. Decimal 9 = 0x0A and ASCII 'A'= Decimal 48 (So subtract 48) */
		/* Decimal 10 = 0x0A and ASCII 'A'= Decimal 65 (So subtract 55) */
		if(UpperNibble >= 65)
			UpperNibble-=55;

		else
			UpperNibble-=48;

		/* Repeat Upper nibble ASCII conversion for the lower
		 * nibble */
		if(LowerNibble >= 65)
			LowerNibble-=55;

		else
			LowerNibble-=48;

		/* Bit shift to makes upper nibble truly the upper nibble */
		UpperNibble <<= 4;

		/* Place in output buffer (OR creates the original byte */
		RawByteOut[OutIndex]	= UpperNibble | LowerNibble;

		/* Move to the next index for upper and lower nibbles */
		OutIndex+=1;

		/* Increment array in ASCII bytes */
		i+=2;
	}


	return RawByteCnt;

}
