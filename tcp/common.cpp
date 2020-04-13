#include "common.h"



bool tcp::Common::get_next_packet(Packet& packet)
{
	return packets->try_pop(packet);
}

void tcp::Common::log_socket_error(std::string msg)
{
#ifdef _WIN32
	int err_code = WSAGetLastError();
	wchar_t *s = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				   nullptr, (DWORD)err_code,
				   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				   (LPWSTR)&s, 0, nullptr);

	std::string error_str;
	while(*s) error_str += (char)*s++;

	log_error(msg + " (" + std::to_string(err_code) + "): " + error_str);

	LocalFree(s);
#else
	perror(msg);
#endif
}

void tcp::Common::log_error(std::string msg)
{
	std::cout << msg << std::endl;
}

int tcp::Common::close_socket(SOCKET sock)
{
	int status = 0;

#ifdef _WIN32
	status = shutdown(sock, SD_BOTH);
	if (status == 0) { status = closesocket(sock); }
#else
	status = shutdown(sock, SHUT_RDWR);
	if (status == 0) { status = close(sock); }
#endif

	return status;
}
