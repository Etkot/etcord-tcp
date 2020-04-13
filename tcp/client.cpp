#include "client.h"



tcp::Client::Client()
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup(MAKEWORD(1,1), &wsa_data);
#endif
}

tcp::Client::~Client()
{
	disconnect();

#ifdef _WIN32
	WSACleanup();
#endif
	delete packets;
}

bool tcp::Client::connect(const char* address, const uint16_t& port)
{
	if (status == Status::Connected)
	{
		return false;
	}

	sock = 0;
	struct sockaddr_in serv_addr;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		log_socket_error("Socket creation error");

		status = tcp::Status::Unconnected;
		return false;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, address, &serv_addr.sin_addr) <= 0)
	{
		log_socket_error("Invalid address/ Address not supported");

		status = tcp::Status::Unconnected;
		return false;
	}

	if (::connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		log_socket_error("Connection Failed");

		status = tcp::Status::Unconnected;
		return false;
	}

	// Initialize message queue
	packets = new SafeQueue<Packet>();

	// Start reading incoming packets on a new thread
	status = tcp::Status::Connected;
	read_thread = new std::thread(&Client::read, this);

	return true;
}

void tcp::Client::disconnect()
{
	status = Status::Disconnected;

#ifndef _WIN32
	write(stop_pipe, "0", 1);
#endif

	read_thread->join();
	delete read_thread;

	packets->push({ sock, PacketType::Disconnect, nullptr, 0 });

	close_socket(sock);
}


bool tcp::Client::send(const char* message, const uint16_t& length)
{
	char* packet = new char[length + 2];
	memcpy(packet + 2, message, length);
	packet[0] = (char)(length >> 8);
	packet[1] = (char)length;

	int res = ::send(sock, packet, length + 2, 0) == length + 2;

	delete[] packet;
	return res;
}



void tcp::Client::read()
{
	int activity, valread;
	SOCKET max_sd;

	//set of socket descriptors
	fd_set readfds;

	// For reading packets
	char header[2];
	uint16_t payload_size;

#ifndef _WIN32
	// Setup pipe for stopping
	int pipefd[2];
	if (::pipe(pipefd) == -1) {
		perror("pipe");
		return;
	}
	stop_pipe = pipefd[1];
#endif

	packets->push({ sock, PacketType::Connect, nullptr, 0 });

	while(status == Status::Connected)
	{
		// Clear the socket set
		FD_ZERO(&readfds);

		// Add socket to set
		FD_SET(sock, &readfds);
		max_sd = sock;

#ifndef _WIN32
		// Add stop pipe to set
		FD_SET(pipefd[0], &readfds);
		if (pipefd[0] > max_sd)
			max_sd = pipefd[0];
#endif

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
			log_socket_error("select error");
		}

		if (FD_ISSET(sock, &readfds))
		{
			// Check if it was for closing, and also read the
			// incoming message
			if ((valread = ::recv(sock, header, 2, NULL)) == 0)
			{
				// Client disconnected
				packets->push({ sock, PacketType::Disconnect, nullptr, 0 });

				// Close the socket and mark as 0 in list for reuse
				close_socket(sock);
			}
			else
			{
				if (valread != 2) {
					log_socket_error("Server sent an invalid header");
				}

				// Header should be big endian
				payload_size = (uint16_t)((header[0] << 8) + header[1]);

				if (payload_size > 0)
				{
					char* buffer = new char[payload_size];
					if ((valread = ::recv(sock, buffer, payload_size, NULL)) != 0)
					{
						if (valread != payload_size)
						{
							log_socket_error("The whole payload wasn't received");
						}
						else
						{
							packets->push({ sock, PacketType::Message, buffer, payload_size });
						}
					}
				}
			}
		}
	}
}
