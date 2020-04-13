#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "common.h"

namespace tcp {
	class Server;
}

class tcp::Server: public Common
{
public:
	// Constructor and deconstructor
	Server();
	~Server();


	// Methods
	bool start(const uint16_t& port);
	void stop();

	std::string get_client_address(const SOCKET& sd);

	bool send(const SOCKET& socket, const char* message, const uint16_t& length);

private:
	// Variables
	struct sockaddr_in address;
	int addrlen;

	SOCKET master_socket;

	std::vector<SOCKET> client_sockets;

	bool running = false;
	std::thread* listen_thread;
	int stop_pipe;


	// Methods
	void listen();
	void close_all_client_connections();
};

#endif // TCPSERVER_H
