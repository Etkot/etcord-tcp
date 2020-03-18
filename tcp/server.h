#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <stdint.h>
#include <netinet/in.h>
#include <vector>
#include <thread>
#include "safequeue.h"
#include "packet.h"

namespace tcp {
	class Server;
}

class tcp::Server
{
public:
	// Constructor and deconstructor
	Server(uint max_clients) : max_clients(max_clients) {}
	~Server();


	// Variables
	std::vector<int> client_sockets;
	SafeQueue<Packet>* messages;

	// Methods
	bool start(uint16_t port);

	bool send_to_client(int socket, char* message, int length);
	bool send_to_all(char* message, int length, int socket_excluded = 0);

private:
	// Variables
	uint16_t port;
	uint max_clients;

	struct sockaddr_in address;
	int addrlen;

	int master_socket;

	bool running;
	std::thread* listen_thread;


	// Methods
	void listen();
	void handle_new_connection();

	void close_all_connections();
};

#endif // TCPSERVER_H
