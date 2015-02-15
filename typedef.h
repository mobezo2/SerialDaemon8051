/*
 * typedef.h
 *
 *  Created on: Sep 9, 2014
 *      Author: mbezold
 */


#ifndef TYPEDEF_H_
#define TYPEDEF_H_

#define LINUX
#define ARM

typedef signed char 	sint8_t ;
typedef unsigned char 	uint8_t;
typedef short 			int16_t;
typedef unsigned short 	uint16_t;
typedef int 			int32_t;
typedef unsigned int 	uint32_t;

typedef unsigned long 	uint64_t;
typedef long long 		int64_t;
typedef float     		float32_t;
typedef double	  		float64_t;

/* Used to get rid of compiler warnings, a char in arm is unsigned, but
 * using the unsigned key word above for clarity and portability leads
 * to compiler warnings */
#ifdef ARM
	typedef char ARM_char_t;
#else
	typedef unsigned char ARM_char_t;
#endif //ARM


#ifndef LINUX
	typedef unsigned short size_t
#endif



#endif /* TYPEDEF_H_ */
