#include <iostream>
#include <stdio.h>

#include "tcp/server.h"
#include "tcp/client.h"


void start_server(uint16_t port, uint max_clients) {
	std::cout << "Starting server on port " << port << " with " << (max_clients == 0 ? "infinite" : std::to_string(max_clients)) << " clients able to connect" << std::endl;

	tcp::Server *server = new tcp::Server(max_clients);
	server->start(port);

	tcp::Packet packet;
	while (true) {
		while (server->messages->count()) {
			packet = server->messages->pop();

			if (packet.type == tcp::PacketType::Connect) {
				std::cout << "Client connected" << std::endl;
			}
			else if (packet.type == tcp::PacketType::Disconnect) {
				std::cout << "Client disconnect" << std::endl;
			}
			else if (packet.type == tcp::PacketType::Message) {
				std::cout << "Client send a message" << std::endl;
			}
		}
	}
}
void start_client(std::string address, uint16_t port) {
	std::cout << "Connecting to " << address << ":" << port << std::endl;

	tcp::Client *client = new tcp::Client();
	client->connect(address.c_str(), port);


}

int main()
{
	bool is_client = true;
	std::string address;
	uint16_t port = 7357;
	uint max_clients = 0;


	std::string input;
	std::cout << "Start client or server (c/s): ";
	std::getline(std::cin, input);

	if (input == "s") {
		is_client = false;
	}

	if (is_client) {
		std::cout << "Server address and port (127.0.0.1:7357): ";
		std::getline(std::cin, input);

		if (input.length() != 0) {
			try {
				address = input.substr(0, input.find(":"));
				port = std::stoi(input.substr(input.find(":") + 1, input.length()));
			} catch (std::invalid_argument) {
				std::cout << "Invalid address" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}
	else {
		std::cout << "Server port (7357): ";
		std::getline(std::cin, input);

		if (input.length() != 0) {
			try {
				port = std::stoi(input);
			} catch (std::invalid_argument) {
				std::cout << "Invalid port: " << std::endl;
				return EXIT_FAILURE;
			}
		}

		std::cout << "Maximum clients (infinite): ";
		std::getline(std::cin, input);

		if (input.length() != 0) {
			try {
				max_clients = std::stoi(input);
			} catch (std::invalid_argument) {
				std::cout << "Invalid client count" << std::endl;
				return EXIT_FAILURE;
			}
		}
	}


	if (is_client) {
		start_client(address, port);
	} else {
		start_server(port, max_clients);
	}

	return EXIT_SUCCESS;
}
