#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <string>

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

	bool connect(const char *address, uint16_t &port);
	void disconnect();
	bool send(char *message, int length);
	int read(char *buffer, int size);

	Status get_status() { return status_; }

private:
	int sock_ = 0;
	Status status_;
};

#endif // TCPCLIENT_H
