#ifndef SOCKET_H
#define SOCKET_H

#undef UNICODE
#undef _UNICODE
#include "Platform.h"
#ifdef WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#endif

class Socket {
public:
	int ResolveAddress(tstring address, tstring port);
	int Connect(tstring address, tstring port);
	void CloseSocket();
	int GetStatus();
	Socket();
	~Socket();
private:
	SOCKET sock;
	addrinfo* lastaddrinfo;
	int status;
};


#endif