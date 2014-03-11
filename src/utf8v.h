#ifndef _UTF8V_H_4255
#define _UTF8V_H_4255

#include <stdlib.h>

/*	Convert src from Latin-1 to UTF-8.
	Returns number of bytes needed to convert src from Latin-1 to UTF-8.
	Copies into utf8buf as many characters that will fit from src based on utf8buflen.
	If utf8buflen is not large enough, the return value will be larger than utf8buflen.
	If there is room, a nul byte will be placed after the end (not included in return count).
*/
size_t latin1toUTF8(const void *src, size_t srclength, void *utf8buf, size_t utf8buflen);

/* Returns 0 if invalid. */
int isValidUTF8String(const void *utf8, size_t length);

#define INVALID_UTF8 (UTF8)0xFF

/* utf8buf filled with the contents of the next character.
	utf8buf must contain at least 4 bytes.
	Trailing elements set to INVALID_UTF8 are not part of the character.
	Note: if the char takes 4 utf8 bytes, a 0 byte is NOT placed after it.
	All elements are set to INVALID_UTF8 if bad conversion, end of string, etc.
	Returns number of valid bytes added to utf8buf.
*/
int UTF32toUTF8char(unsigned long utf32char, void* utf8buf);

#define HAS_UTF32toUTF8char

#endif
