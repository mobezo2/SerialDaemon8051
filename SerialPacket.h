/*
 * SerialPacket.h
 *
 *  Created on: Aug 18, 2014
 *      Author: mbezold
 */

#ifndef SERIALPACKET_H_
#define SERIALPACKET_H_


typedef struct {
		char TimeSent[27];
		char TimeReceived[27];
		char MsgLength;
		char DataField[30];

}SerialPacket;


#endif /* SERIALPACKET_H_ */
