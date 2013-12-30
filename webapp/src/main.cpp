/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"

using namespace std;

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "");
	#ifdef __GNUC__
	setvbuf(stdout, NULL, _IONBF, 0);
	#endif

	asio::io_service svc;
	Webapp app(svc);

	return app.Start();
}

