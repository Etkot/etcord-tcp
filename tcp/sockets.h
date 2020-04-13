#ifndef SOCKETS_H
#define SOCKETS_H



#ifdef _WIN32
	#pragma comment (lib, "Ws2_32.lib")

	/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
	#ifndef _WIN32_WINNT
		//#define _WIN32_WINNT 0x0501  /* Windows XP. */
	#endif
	#include <WinSock2.h>
	#include <WS2tcpip.h>
#else
	/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netinet/in.h>

typedef int SOCKET;
#endif



#endif // SOCKETS_H
