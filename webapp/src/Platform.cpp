/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Platform.h"

using namespace std;

/**
 * Signed number in string form to int32_t with a maximum size, n.
 * @param src the number in string form
 * @n the maximum number of characters to process
*/
int32_t strntol(const char* src, size_t n)
{
	int32_t x = 0;
	int32_t multiplier = 1;
	if(*src == '-' && n--) 
	{
		multiplier *= -1;
		src++;
	}
	while(isdigit(*src) && n--) {
		x = x * 10 + (*src - '0');
		src++;
	}
	return x * multiplier;
}
