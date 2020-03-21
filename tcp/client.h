#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <string>

#include "packet.h"
#include "safequeue.h"

namespace tcp {
	enum class Status;
	class Client;
}

enum class tcp::Status { Unconnected=0, Connected=1, Disconnected=2 };

class tcp::Client
{
public:
	Client();
	~Client();


	// Methods
	bool connect(const char *address, uint16_t &port);
	void disconnect();

	bool get_next_packet(Packet& packet);
	bool send(char *message, uint16_t length);

	Status get_status() { return status_; }

private:
	// Variables
	int sock_ = 0;
	Status status_;

	std::thread* read_thread;
	int stop_pipe;

	SafeQueue<Packet>* packets;


	// Methods
	void read();
};

#endif // TCPCLIENT_H
