#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <thread>

#include <string>
#include <iostream>

#include "sockets.h"
#include "safequeue.h"
#include "packet.h"



namespace tcp {
	class Common;
}

class tcp::Common
{
public:
	// Methods
	bool get_next_packet(Packet& packet);

	void set_log_function(void(*f)(std::string));

protected:
	// Methods
	int close_socket(const SOCKET& sock);

	void log_socket_error(const std::string& msg);
	void log_error(const std::string& msg);


	// Variables
	SafeQueue<Packet>* packets;

	void(*log_func)(std::string) = nullptr;
};


#endif // COMMON_H
