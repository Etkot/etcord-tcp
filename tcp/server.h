#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <stdint.h>
#include <netinet/in.h>
#include <vector>
#include <unordered_map>
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
	Server() {}
	~Server();


	// Methods
	bool start(uint16_t& port);
	void stop();

	bool get_next_packet(Packet& packet);
	std::string get_client_address(int sd);

	bool send(int socket, char* message, uint16_t length);

private:
	// Variables
	uint16_t port;

	struct sockaddr_in address;
	int addrlen;

	int master_socket;

	std::vector<int> client_sockets;
	SafeQueue<Packet>* packets;

	bool running;
	std::thread* listen_thread;
	int stop_pipe;


	// Methods
	void listen();
	void close_all_client_connections();
};

#endif // TCPSERVER_H
