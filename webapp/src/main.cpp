
#include "Webapp.h"


using namespace std;

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "");
	#ifdef __GNUC__
	setvbuf(stdout, NULL, _IONBF, 0);
	#endif

	asio::io_service svc;
	Webapp* gallery = new Webapp(svc);
		
	delete gallery;
	return 0;
}

