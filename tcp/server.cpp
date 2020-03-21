#include "server.h"
#include <stdio.h>
#include <string.h>		//strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>		//close
#include <arpa/inet.h>	//close
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>	//FD_SET, FD_ISSET, FD_ZERO macros
#include <algorithm>
#include <map>

#define TRUE   1
#define FALSE  0

namespace tcp {
	struct PacketBuffer
	{
		char* data;
		int length;
		int packet_length;
	};
}



tcp::Server::~Server()
{
	stop();
	delete packets;
}

bool tcp::Server::start(uint16_t& port)
{
	if (running) {
		return false;
	}

	int opt = TRUE;

	// Create a master socket
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
	{
		perror("socket failed");
		return false;
	}

	// Set master socket to allow multiple connections ,
	// this is just a good habit, it will work without this
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
	{
		perror("setsockopt");
		return false;
	}

	// Type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( port );

	// Bind the socket to localhost port
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
	{
		perror("bind failed");
		return false;
	}
	//printf("listener on port %d \n", port);

	// Try to specify maximum of 3 pending connections for the master socket
	if (::listen(master_socket, 3) < 0)
	{
		perror("listen");
		return false;
	}

	addrlen = sizeof(address);

	// Initialize message queue
	packets = new SafeQueue<Packet>();

	// Start listening for incoming packets on a new thread
	running = true;
	listen_thread = new std::thread(&Server::listen, this);

	return true;
}

void tcp::Server::stop()
{
	if (!running) {
		return;
	}

	close_all_client_connections();
	//close(master_socket);

	running = false;
	write(stop_pipe, "0", 1);
	listen_thread->join();
	delete listen_thread;
}


bool tcp::Server::get_next_packet(Packet& packet) {
	return packets->try_pop(packet);
}

std::string tcp::Server::get_client_address(int sd) {
	struct sockaddr_in addr;
	int _addrlen;

	getpeername(sd, (struct sockaddr*)&addr, (socklen_t*)&_addrlen);
	return std::string(inet_ntoa(addr.sin_addr));
}


bool tcp::Server::send(int socket, char* message, uint16_t length)
{
	char packet[length + 2];
	memcpy(packet + 2, message, length);
	packet[0] = length >> 8;
	packet[1] = length;

	return ::send(socket, packet, length + 2, 0) == length;
}





void tcp::Server::listen()
{
	int activity, valread, sd, max_sd, new_socket;
	uint i;

	//set of socket descriptors
	fd_set readfds;

	// For reading packets
	char header[2];
	uint16_t payload_size;

	// Setup pipe for stopping
	int pipefd[2];
	if (::pipe(pipefd) == -1) {
		perror("pipe");
		return;
	}
	stop_pipe = pipefd[1];

	while(running)
	{
		// Clear the socket set
		FD_ZERO(&readfds);

		// Add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

		// Add stop pipe to set
		FD_SET(pipefd[0], &readfds);
		if (pipefd[0] > max_sd)
			max_sd = pipefd[0];

		// Add client sockets to set
		for (i = 0; i < client_sockets.size(); i++)
		{
			FD_SET(client_sockets[i], &readfds);
			if (client_sockets[i] > max_sd)
				max_sd = client_sockets[i];
		}

		// Wait for an activity on one of the sockets, timeout is NULL,
		// so wait indefinitely
		// (change to epoll?)
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno!=EINTR))
		{
			printf("select error");
		}

		// Check IO operations from sockets before accepting new sockets
		// so that they arent automatically set
		for (i = 0; i < client_sockets.size(); i++)
		{
			sd = client_sockets[i];
			if (FD_ISSET(sd, &readfds))
			{
				// Check if it was for closing, and also read the
				// incoming message
				if ((valread = ::read(sd, header, 2)) == 0)
				{
					// Somebody disconnected
					packets->push({ sd, PacketType::Disconnect, nullptr, 0 });

					// Close the socket and mark as 0 in list for reuse
					close(sd);
					client_sockets[i] = 0;
				}
				else
				{
					if (valread != 2) {
						printf("Client sent an invalid header\n");
					}

					// Header should be big endian
					payload_size = (header[0] << 8) + header[1];

					if (payload_size > 0)
					{
						char* buffer = new char[payload_size];
						if ((valread = ::read(sd, buffer, payload_size)) != 0)
						{
							if (valread != payload_size)
							{
								printf("The whole payload wasn't received\n");
							}
							else
							{
								packets->push({ sd, PacketType::Message, buffer, payload_size });
							}
						}
					}
				}
			}
		}

		//If something happened on the master socket,
		//then its an incoming connection
		if (FD_ISSET(master_socket, &readfds))
		{
			if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
			{
				perror("accept");
			}

			client_sockets.push_back(new_socket);
			packets->push({ new_socket, PacketType::Connect, nullptr, 0 });
		}

		// Remove clients that disconnected
		client_sockets.erase(
			std::remove_if(
				client_sockets.begin(),
				client_sockets.end(),
				[](int const& s) { return s == 0; }
			),
			client_sockets.end()
		);
	}
}



void tcp::Server::close_all_client_connections()
{
	for (uint i = 0; i < client_sockets.size(); i++) {
		close(client_sockets[i]);
	}

	client_sockets.clear();
}
