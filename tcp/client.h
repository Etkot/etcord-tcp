#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "common.h"

namespace tcp {
	enum class Status;
	class Client;
}

enum class tcp::Status { Unconnected=0, Connected=1, Disconnected=2 };

class tcp::Client: public Common
{
public:
	Client();
	~Client();


	// Methods
	bool connect(const char* address, const uint16_t& port);
	void disconnect();

	bool send(const char* message, const uint16_t& length);

	Status get_status() { return status; }

private:
	// Variables
	SOCKET sock = 0;
	Status status = Status::Unconnected;

	std::thread* read_thread;
	int stop_pipe;


	// Methods
	void read();
};

#endif // TCPCLIENT_H
