#include "server.h"

#include <stdio.h>
#include <string.h>		//strlen
#include <stdlib.h>
#include <errno.h>

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


tcp::Server::Server()
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(1,1), &wsa_data);
#endif
}

tcp::Server::~Server()
{
	stop();

#ifdef _WIN32
	WSACleanup();
#endif

	delete packets;
}

bool tcp::Server::start(const uint16_t& port)
{
	if (running) {
		return false;
	}

	int opt = TRUE;

	// Create a master socket
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
	{
		log_socket_error("socket failed");
		return false;
	}

	// Set master socket to allow multiple connections ,
	// this is just a good habit, it will work without this
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
	{
		log_socket_error("setsockopt");

		return false;
	}

	// Type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( port );

	// Bind the socket to localhost port
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0)
	{
		log_socket_error("bind failed");
		return false;
	}
	//printf("listener on port %d \n", port);

	// Try to specify maximum of 3 pending connections for the master socket
	if (::listen(master_socket, 3) < 0)
	{
		log_socket_error("listen");
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

	running = false;

	close_all_client_connections();
	//close(master_socket);

#ifndef _WIN32
	write(stop_pipe, "0", 1);
#endif

	listen_thread->join();
	delete listen_thread;
}


std::string tcp::Server::get_client_address(const SOCKET& sd) {
	struct sockaddr_in addr;
	int _addrlen;

	getpeername(sd, (struct sockaddr*)&addr, (socklen_t*)&_addrlen);

	return std::string(inet_ntoa(addr.sin_addr));
}


bool tcp::Server::send(const SOCKET& socket, const char* message, const uint16_t& length)
{
	char* packet = new char[length + 2];
	memcpy(packet + 2, message, length);
	packet[0] = (char)(length >> 8);
	packet[1] = (char)length;

	int res = ::send(socket, packet, length + 2, 0) == length;

	delete[] packet;
	return res;
}





void tcp::Server::listen()
{
	int activity, valread;
	SOCKET sd, max_sd, new_socket;
	unsigned int i;

	//set of socket descriptors
	fd_set readfds;

	// For reading packets
	char header[2];
	uint16_t payload_size;

#ifndef _WIN32
	// Setup pipe for stopping
	int pipefd[2];
	if (::pipe(pipefd) == -1) {
		log_error("pipe")
		return;
	}
	stop_pipe = pipefd[1];
#endif

	while(running)
	{
		// Clear the socket set
		FD_ZERO(&readfds);

		// Add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

#ifndef _WIN32
		// Add stop pipe to set
		FD_SET(pipefd[0], &readfds);
		if (pipefd[0] > max_sd)
			max_sd = pipefd[0];
#endif

		// Add client sockets to set
		for (i = 0; i < client_sockets.size(); i++)
		{
			FD_SET(client_sockets[i], &readfds);
			if (client_sockets[i] > max_sd)
				max_sd = client_sockets[i];
		}

		// Wait for an activity on one of the sockets, timeout is NULL, so wait indefinitely
		// (change to epoll?)
#ifdef _WIN32
		timeval time;
		time.tv_sec = 1;
		time.tv_usec = 0;
		activity = select((int)max_sd + 1, &readfds, nullptr, nullptr, &time);
#else
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
#endif

		if ((activity < 0) && (errno!=EINTR))
		{
			printf("select error");
		}

		if (!running) break;

		// Check IO operations from sockets before accepting new sockets
		// so that they arent automatically set
		for (i = 0; i < client_sockets.size(); i++)
		{
			sd = client_sockets[i];
			if (FD_ISSET(sd, &readfds))
			{
				// Check if it was for closing, and also read the
				// incoming message
				if ((valread = ::recv(sd, header, 2, NULL)) == -1)
				{
					log_socket_error("recv error");
				}
				else if (valread == 0)
				{
					// Somebody disconnected
					packets->push({ sd, PacketType::Disconnect, nullptr, 0 });

					// Close the socket and mark as 0 in list for reuse
					close_socket(sd);
					client_sockets[i] = 0;
				}
				else
				{
					if (valread != 2) {
						log_error("Client sent a header with invalid size");
					}

					// Header should be big endian
					payload_size = (uint16_t)((header[0] << 8) + header[1]);

					if (payload_size > 0)
					{
						char* buffer = new char[payload_size];
						if ((valread = ::recv(sd, buffer, payload_size, NULL)) != 0)
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
				log_socket_error("accept");
			}

			client_sockets.push_back(new_socket);
			packets->push({ new_socket, PacketType::Connect, nullptr, 0 });
		}

		// Remove clients that disconnected
		client_sockets.erase(
			std::remove_if(
				client_sockets.begin(),
				client_sockets.end(),
				[](SOCKET const& s) { return s == 0; }
			),
			client_sockets.end()
		);
	}
}



void tcp::Server::close_all_client_connections()
{
	for (unsigned int i = 0; i < client_sockets.size(); i++) {
		close_socket(client_sockets[i]);
	}

	client_sockets.clear();
}
