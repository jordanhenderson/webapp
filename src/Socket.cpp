#include "Socket.h"
using namespace std;
int Socket::ResolveAddress(string address, string port) {
	struct addrinfo *result = NULL,
					*ptr = NULL,
					hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int retVal = getaddrinfo(address.c_str(), port.c_str(), &hints, &result);

	if(lastaddrinfo != NULL)
		freeaddrinfo(lastaddrinfo);

	lastaddrinfo = result;

	return retVal;
}

int Socket::Connect(string address, string port) {
	int retVal = this->ResolveAddress(address, port);
	if(retVal || lastaddrinfo == NULL)
		return retVal;

	//Attempt connection to each addr in addr_info
	addrinfo* res;
	for(res = lastaddrinfo; res != NULL; res = res->ai_next) {
		retVal = connect(sock, lastaddrinfo->ai_addr, (int)lastaddrinfo->ai_addrlen);
		if(retVal != SOCKET_ERROR)
			break;
	}

	if(retVal == SOCKET_ERROR) {
		CloseSocket();
		status = SOCKET_ERROR;
	} else {
		status = 0;
	}

	return status;
}

int Socket::GetStatus() {
	return status;
}

void Socket::CloseSocket() {
	if(sock != INVALID_SOCKET)
		closesocket(sock);
	sock = INVALID_SOCKET;
}

Socket::Socket() {
	sock = INVALID_SOCKET;
}

Socket::~Socket() {
	if(sock != INVALID_SOCKET)
		closesocket(sock);
}