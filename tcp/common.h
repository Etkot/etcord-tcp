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

protected:
	// Methods
	int close_socket(SOCKET sock);
	void log_socket_error(std::string msg);
	void log_error(std::string msg);


	// Variables
	SafeQueue<Packet>* packets;
};


#endif // COMMON_H
