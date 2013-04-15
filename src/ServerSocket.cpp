#include "ServerSocket.h"
#include "Gallery.h"
int ServerSocket::GetStatus() {
	return status;
}

void ServerSocket::CloseSocket() {
	if(sock != INVALID_SOCKET)
		closesocket(sock);
	sock = INVALID_SOCKET;
}

ServerSocket::ServerSocket(Logging*) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock == INVALID_SOCKET) {
		nError = ERROR_SOCKET_FAILED;
#ifdef WIN32
		WSACleanup();
#endif
		return;
	}



	
}

ServerSocket::~ServerSocket() {
	if(sock != INVALID_SOCKET)
		closesocket(sock);
#ifdef WIN32
	WSACleanup();
#endif
}