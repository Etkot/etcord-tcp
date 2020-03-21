#include "client.h"
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>



tcp::Client::Client() {}

tcp::Client::~Client()
{
	disconnect();
	delete packets;
}

bool tcp::Client::connect(const char *address, uint16_t &port)
{
	sock_ = 0;
	struct sockaddr_in serv_addr;
	if ((sock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");

		status_ = tcp::Status::Unconnected;
		return false;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, address, &serv_addr.sin_addr)<=0)
	{
		printf("Invalid address/ Address not supported\n");


		status_ = tcp::Status::Unconnected;
		return false;
	}

	if (::connect(sock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Connection Failed\n");


		status_ = tcp::Status::Unconnected;
		return false;
	}

	// Initialize message queue
	packets = new SafeQueue<Packet>();

	// Start reading incoming packets on a new thread
	status_ = tcp::Status::Connected;
	read_thread = new std::thread(&Client::read, this);

	return true;
}

void tcp::Client::disconnect()
{
	status_ = Status::Disconnected;

	write(stop_pipe, "0", 1);
	read_thread->join();
	delete read_thread;

	packets->push({ sock_, PacketType::Disconnect, nullptr, 0 });

	close(sock_);
}

bool tcp::Client::get_next_packet(Packet& packet) {
	return packets->try_pop(packet);
}

bool tcp::Client::send(char *message, uint16_t length)
{
	char packet[length + 2];
	memcpy(packet + 2, message, length);
	packet[0] = length >> 8;
	packet[1] = length;

	return ::send(sock_, packet, length + 2, 0) == length + 2;
}



void tcp::Client::read()
{
	int activity, valread, max_sd;

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


	// Clear the socket set
	FD_ZERO(&readfds);

	// Add socket to set
	FD_SET(sock_, &readfds);
	max_sd = sock_;

	// Add stop pipe to set
	FD_SET(pipefd[0], &readfds);
	if (pipefd[0] > max_sd)
		max_sd = pipefd[0];

	packets->push({ sock_, PacketType::Connect, nullptr, 0 });

	while(status_ == Status::Connected)
	{
		// Wait for an activity on one of the sockets, timeout is NULL,
		// so wait indefinitely
		// (change to epoll?)
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno!=EINTR))
		{
			printf("select error");
		}

		if (FD_ISSET(sock_, &readfds))
		{
			// Check if it was for closing, and also read the
			// incoming message
			if ((valread = ::read(sock_, header, 2)) == 0)
			{
				// Client disconnected
				packets->push({ sock_, PacketType::Disconnect, nullptr, 0 });

				// Close the socket and mark as 0 in list for reuse
				close(sock_);
			}
			else
			{
				if (valread != 2) {
					printf("Server sent an invalid header\n");
				}

				// Header should be big endian
				payload_size = (header[0] << 8) + header[1];

				if (payload_size > 0)
				{
					char* buffer = new char[payload_size];
					if ((valread = ::read(sock_, buffer, payload_size)) != 0)
					{
						if (valread != payload_size)
						{
							printf("The whole payload wasn't received\n");
						}
						else
						{
							packets->push({ sock_, PacketType::Message, buffer, payload_size });
						}
					}
				}
			}
		}
	}
}
