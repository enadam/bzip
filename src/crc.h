/* crc.h */
#ifndef CRC_H
#define CRC_H

/* Include files */
#include "config.h"

#include <sys/types.h>

/* Macros */
/*------------------------------------------------------*/
/* I think this is an implementation of the AUTODIN-II,	*/
/* Ethernet & FDDI 32-bit CRC standard.  Vaguely	*/
/* derived from code by Rob Warnock, in Section 51 of	*/
/* the comp.compression FAQ.				*/
/*------------------------------------------------------*/
#define updateCRC(crc, cha) \
	((crc) = (((crc) << 8) ^ crc32Table[((crc) >> 24) ^ (cha)]))

/* Global variables */
extern u_int32_t const crc32Table[];

#endif /* CRC_H */
