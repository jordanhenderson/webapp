#ifndef SERVER_SOCKET_H
#define SERVER_SOCKET_H

#undef UNICODE
#undef _UNICODE
#include "Platform.h"
#include "Logging.h"
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

class Gallery;

class ServerSocket : Internal {
public:
	void CloseSocket();
	int GetStatus();
	ServerSocket(Logging*);
	~ServerSocket();
private:
	SOCKET sock;
	addrinfo* lastaddrinfo;
	int status;
};


#endif