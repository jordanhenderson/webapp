// :mode=c++:
/*
decode.h - c++ wrapper for a base64 decoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#define BUFFERSIZE 16777216
namespace base64
{
	extern "C"
	{
		#include "cdecode.h"
	}

	struct decoder
	{
		base64_decodestate _state;
		int _buffersize;

		decoder(int buffersize_in = BUFFERSIZE)
		: _buffersize(buffersize_in)
		{}

		int decode(char value_in)
		{
			return base64_decode_value(value_in);
		}

		int decode(const char* code_in, const int length_in, char* plaintext_out)
		{
			return base64_decode_block(code_in, length_in, plaintext_out, &_state);
		}
	};

} // namespace base64



#endif // BASE64_DECODE_H

