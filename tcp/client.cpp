#include "client.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>



tcp::Client::Client() {}

tcp::Client::~Client() {}

bool tcp::Client::connect(const char *address, uint16_t &port)
{
	sock_ = 0;
	struct sockaddr_in serv_addr;
	if ((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		//log_print("Socket creation error");

		status_ = tcp::Status::Unconnected;
		return false;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, address, &serv_addr.sin_addr)<=0)
	{
		//log_print("Invalid address/ Address not supported");

		status_ = tcp::Status::Unconnected;
		return false;
	}

	if (::connect(sock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		//log_print("Connection Failed");

		status_ = tcp::Status::Unconnected;
		return false;
	}

	status_ = tcp::Status::Connected;
	return true;
}

void tcp::Client::disconnect()
{
	close(sock_);
	sock_ = 0;
	status_ = Status::Disconnected;
}

bool tcp::Client::send(char *message, int length)
{
	return ::send(sock_, message, length, 0) == length;
}

int tcp::Client::read(char *buffer, int size)
{
	return ::read(sock_, buffer, size);
}
