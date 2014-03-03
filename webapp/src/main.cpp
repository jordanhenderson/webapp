/* Copyright (C) Jordan Henderson - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Jordan Henderson <jordan.henderson@ioflame.com>, 2013
 */

#include "Webapp.h"
#include "SimpleOpt.h"

using namespace std;

struct tm epoch_tm = {
    0,0,0,1,0,70,0,0,0
};

//Calculate epoch for portability purposes.
time_t epoch = mktime(&epoch_tm);

CSimpleOpt::SOption webapp_options[] = {
    {WEBAPP_OPT_SESSION, "--session", SO_REQ_CMB},
    SO_END_OF_OPTIONS
};

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "");
	#ifdef __GNUC__
	setvbuf(stdout, NULL, _IONBF, 0);
	#endif

    CSimpleOptA args(argc, argv, webapp_options);
    const char* session = WEBAPP_OPT_SESSION_DEFAULT;
    while(args.Next()) {
        ESOError err = args.LastError();
        if(err == SO_SUCCESS) {
            switch(args.OptionId()) {
                case WEBAPP_OPT_SESSION:
                session = args.OptionArg();
                break;
            }
        }
        else if(err == SO_ARG_MISSING || err == SO_ARG_INVALID_DATA) {
            printf("Error. Usage: webapp [--session=directory]\n");
            return 1;
        }
    }

	asio::io_service svc;
    Webapp app(session, svc);

	app.Start();
	return 0;
}
