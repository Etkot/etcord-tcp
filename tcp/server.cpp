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
	close_all_connections();

	running = false;
	listen_thread->join();
	delete listen_thread;

	delete messages;
}

bool tcp::Server::start(uint16_t port)
{
	int opt = TRUE;

	//create a master socket
	if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0)
	{
		perror("socket failed");
		return false;
	}

	//set master socket to allow multiple connections ,
	//this is just a good habit, it will work without this
	if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
	{
		perror("setsockopt");
		return false;
	}

	//type of socket created
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( port );

	//bind the socket to localhost port
	if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
	{
		perror("bind failed");
		return false;
	}
	printf("listener on port %d \n", port);

	//try to specify maximum of 3 pending connections for the master socket
	if (::listen(master_socket, 3) < 0)
	{
		perror("listen");
		return false;
	}

	addrlen = sizeof(address);

	// Initialize message queue
	messages = new SafeQueue<Packet>();

	// Start listening for incoming packets on a new thread
	running = true;
	listen_thread = new std::thread(&Server::listen, this);

	return true;
}

bool tcp::Server::send_to_client(int socket, char* message, int length)
{
	return send(socket, message, length, 0) == length;
}
bool tcp::Server::send_to_all(char* message, int length, int socket_excluded)
{
	bool success = true;
	for (uint i = 0; i < client_sockets.size(); i++) {
		if (client_sockets[i] == socket_excluded) continue;
		if (send(client_sockets[i], message, length, 0) != length) success = false;
	}
	return success;
}





void tcp::Server::listen()
{
	//accept the incoming connection
	puts("Waiting for connections ...");

	int activity, valread, sd, max_sd;
	uint i;

	//set of socket descriptors
	fd_set readfds;

	std::map<int, PacketBuffer> tempBuffers;

	while(running)
	{
		//clear the socket set
		FD_ZERO(&readfds);

		//add master socket to set
		FD_SET(master_socket, &readfds);
		max_sd = master_socket;

		//add child sockets to set
		for ( i = 0 ; i < client_sockets.size(); i++)
		{
			//socket descriptor
			sd = client_sockets[i];

			//if valid socket descriptor then add to read list
			if (sd > 0)
				FD_SET(sd, &readfds);

			//highest file descriptor number, need it for the select function
			if (sd > max_sd)
				max_sd = sd;
		}

		//wait for an activity on one of the sockets, timeout is NULL,
		//so wait indefinitely
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno!=EINTR))
		{
			printf("select error");
		}

		//If something happened on the master socket,
		//then its an incoming connection
		if (FD_ISSET(master_socket, &readfds))
		{
			handle_new_connection();
		}

		//else its some IO operation on some other socket
		for (i = 0; i < client_sockets.size(); i++)
		{
			sd = client_sockets[i];

			if (FD_ISSET(sd, &readfds))
			{
				//Check if it was for closing, and also read the
				//incoming message

				char* buffer = new char[1024];  //data buffer of 1K
				if ((valread = read(sd, buffer, 1024)) == 0)
				{
					//Somebody disconnected, get his details and print
					getpeername(sd , (struct sockaddr*)&address, (socklen_t*)&addrlen);
					printf("Host disconnected, ip: %s, port: %d \n", inet_ntoa(address.sin_addr), ntohs(address.sin_port));

					const int size = strlen(inet_ntoa(address.sin_addr)) + sizeof(uint16_t);
					uint16_t disconnect_port = ntohs(address.sin_port);
					strcpy(buffer, inet_ntoa(address.sin_addr));
					memcpy(buffer + strlen(inet_ntoa(address.sin_addr)), &disconnect_port, sizeof(uint16_t));
					messages->push({ sd, PacketType::Disconnect, buffer, size });

					//Close the socket and mark as 0 in list for reuse
					close(sd);
					client_sockets[i] = 0;
				}

				//Echo back the message that came in
				else
				{
					if (tempBuffers.find(sd) == tempBuffers.end()) {
						// There is no buffer waiting for more data

						int len = buffer[0];
						len = len << 8;
						len += buffer[1];

						// If the whole packet came in in one read ...
						if (len <= valread - 2) {
							// ... push it to the message queue
							messages->push({ sd, PacketType::Message, buffer, valread });

							// Check if the read had more than one packet
							if (len < valread - 2) {
								// There is more data
							}
						}
						else {
							// The read didn't have the full packet
							tempBuffers.insert({ sd, { buffer, valread, len } });
						}
					}
					else {
						// There is a buffer waiting for more data

						PacketBuffer &packetBuffer = tempBuffers.at(sd);
						//char* new_buffer = new char[packetBuffer.length + valread];

						// If the whole packet has come in ...
						if (packetBuffer.packet_length <= packetBuffer.length + valread) {
							// ... push it to the message queue
							messages->push({ sd, PacketType::Message, buffer, valread });
						}
					}
				}
			}
		}

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

void tcp::Server::handle_new_connection()
{
	int new_socket;

	if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0)
	{
		perror("accept");
	}

	if (client_sockets.size() >= max_clients) {
		char m[] = "Server Full";
		send_to_client(new_socket, m, strlen(m));
		close(new_socket);
		return;
	}

	client_sockets.push_back(new_socket);

	//inform user of socket number - used in send and receive commands
	printf("New connection, socket fd: %d, ip: %s, port: %d\n" , new_socket , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

	const int size = strlen(inet_ntoa(address.sin_addr)) + sizeof(uint16_t);
	char* buffer = new char[size];
	uint16_t connect_port = ntohs(address.sin_port);
	strcpy(buffer, inet_ntoa(address.sin_addr));
	memcpy(buffer + strlen(inet_ntoa(address.sin_addr)), &connect_port, sizeof(uint16_t));
	messages->push({ new_socket, PacketType::Connect, buffer, size });
}



void tcp::Server::close_all_connections()
{
	for (uint i = 0; i < client_sockets.size(); i++) {
		close(client_sockets[i]);
	}

	client_sockets.clear();
}
