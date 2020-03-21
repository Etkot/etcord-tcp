#include <iostream>
#include <stdio.h>

#include "tcp/server.h"
#include "tcp/client.h"


#include <unistd.h>
#include <cstring>


void start_server(uint16_t port)
{
	std::cout << "MAIN: Starting server on port " << port << std::endl;

	tcp::Server *server = new tcp::Server();
	server->start(port);

	tcp::Packet packet;
	while (true)
	{
		while (server->get_next_packet(packet))
		{
			if (packet.type == tcp::PacketType::Connect)
			{
				std::string client_address = server->get_client_address(packet.sender);
				std::cout << "MAIN: Client connected from " << client_address << std::endl;
			}
			else if (packet.type == tcp::PacketType::Disconnect)
			{
				std::cout << "MAIN: Client disconnected" << std::endl;
			}
			else if (packet.type == tcp::PacketType::Message)
			{
				std::string data = std::string(packet.data);

				std::cout << "MAIN: Client send a message: '" << data << "'" << std::endl;

				if (data == "hi") {
					server->send(packet.sender, "hello", 5);
				}
				if (data == "stop")
				{
					std::cout << "MAIN: Stopping" << std::endl;
					server->stop();
					return;
				}

				delete [] packet.data;
			}
		}
	}
}
void start_client(std::string address, uint16_t port)
{
	std::cout << "MAIN: Connecting to " << address << ":" << port << std::endl;

	tcp::Client *client = new tcp::Client();
	bool success = client->connect(address.c_str(), port);

	if (!success) {
		std::cout << "MAIN: Connection failed" << std::endl;
		return;
	}

	tcp::Packet packet;
	while (true)
	{
		while (client->get_next_packet(packet))
		{
			if (packet.type == tcp::PacketType::Connect)
			{
				std::cout << "MAIN: Connected to server" << std::endl;
				client->send("hi", 2);
			}
			else if (packet.type == tcp::PacketType::Disconnect)
			{
				std::cout << "MAIN: Disconnected" << std::endl;
			}
			else if (packet.type == tcp::PacketType::Message)
			{
				std::string data = std::string(packet.data);

				std::cout << "MAIN: Server send a message: '" << data << "'" << std::endl;

				if (data == "hello") {
					client->send("stop", 4);
					client->disconnect();
					return;
				}
				if (data == "stop")
				{
					std::cout << "MAIN: Stopping" << std::endl;
					client->disconnect();
					return;
				}

				delete [] packet.data;
			}
		}
	}
}

int main()
{
	bool is_client = true;
	std::string address = "127.0.0.1";
	uint16_t port = 7357;


	std::string input;
	std::cout << "Start client or server (c/s): ";
	std::getline(std::cin, input);

	if (input == "s")
	{
		is_client = false;
	}

	if (is_client)
	{
		std::cout << "Server address and port (127.0.0.1:7357): ";
		std::getline(std::cin, input);

		if (input.length() != 0)
		{
			try
			{
				address = input.substr(0, input.find(":"));
				port = std::stoi(input.substr(input.find(":") + 1, input.length()));
			}
			catch (std::invalid_argument)
			{
				std::cout << "Invalid address" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}
	else
	{
		std::cout << "Server port (7357): ";
		std::getline(std::cin, input);

		if (input.length() != 0)
		{
			try
			{
				port = std::stoi(input);
			}
			catch (std::invalid_argument)
			{
				std::cout << "Invalid port: " << std::endl;
				return EXIT_FAILURE;
			}
		}
	}


	if (is_client)
	{
		start_client(address, port);
	}
	else
	{
		start_server(port);
	}

	return EXIT_SUCCESS;
}
