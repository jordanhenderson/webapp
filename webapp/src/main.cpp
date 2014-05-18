/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"

using namespace asio;
using namespace std;

struct tm epoch_tm = {
	0,0,0,1,0,70,0,0,0
};

//Calculate epoch for portability purposes.
time_t epoch = mktime(&epoch_tm);

Webapp* app;

int main(int argc, char* argv[])
{
	setlocale(LC_ALL, "");
#ifdef __GNUC__
	setvbuf(stdout, NULL, _IONBF, 0);
#endif

	Webapp _app;
	//Assign address on stack. This is normally dangerous, however this
	//function will only return at the end of execution (shutting down).
	app = &_app; 
	_app.Start();
	return 0;
}
