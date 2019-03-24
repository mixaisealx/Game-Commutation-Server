#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

//Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#include "connection_client.h"
#include <iostream>

void sock_close(SOCKET &socket) {
	closesocket(socket);
}

bool sock_connect_tcp(SOCKET &socket, sockaddr &addr) {
	return connect(socket, &addr, sizeof(addr)) != SOCKET_ERROR;
}

void sock_create_tcp(SOCKET &sock) {
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void sock_create_udp(SOCKET &sock) {
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool sock_prepare_conn(sockaddr &addr, const char* text_addr, unsigned short port) {
	sockaddr_in addrin;
	ZeroMemory(&addrin, sizeof(addr));
	addrin.sin_family = AF_INET;
	addrin.sin_port = htons(port);
	bool valid = inet_pton(AF_INET, text_addr, &addrin.sin_addr.S_un.S_addr) == 1;
	addr = *(sockaddr*)&addrin;
	return valid;
}

int sock_recv_tcp(SOCKET &socket, char *buffer, int buffersize) {
	return recv(socket, buffer, buffersize, 0);
}

int sock_recv_udp(SOCKET &socket, char *buffer, int buffersize) {
	return recvfrom(socket, buffer, buffersize, 0, nullptr, nullptr);
}

void sock_send_tcp(SOCKET &socket, const char *buffer, int buffersize) {
	send(socket, buffer, buffersize, 0);
}

void sock_send_udp(SOCKET &socket, const char *buffer, int buffersize, const sockaddr &addr) {
	sendto(socket, buffer, buffersize, 0, &addr, sizeof(addr));
}

void sock_shutdown_tcp(SOCKET &socket) {
	shutdown(socket, SD_BOTH);
}

void callback_udp(CClient::ClientProtocolProcessor<SOCKET, INVALID_SOCKET, SOCKET_ERROR, sockaddr>::DistributionType distr, char udp_mainTypeUserdata, char userdatd4bits, char *buffer, unsigned short current_bufflen, char unicast_address) {
	std::cout << "received udp: " << current_bufflen <<  " first char: "<< *buffer << std::endl;
}

void start()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) 
		std::cout << "WSA started" << std::endl;

	CClient::ClientProtocolProcessorFunctions<SOCKET, sockaddr> func;
	func.close = sock_close;
	func.conn_tcp = sock_connect_tcp;
	func.create_tcp = sock_create_tcp;
	func.create_udp = sock_create_udp;
	func.prepare_conn = sock_prepare_conn;
	func.recv_tcp = sock_recv_tcp;
	func.recv_udp = sock_recv_udp;
	func.send_tcp = sock_send_tcp;
	func.send_udp = sock_send_udp;
	func.shutdown_tcp = sock_shutdown_tcp;

	CClient::ClientProtocolProcessor<SOCKET, INVALID_SOCKET, SOCKET_ERROR, sockaddr> client(func);
	if (!client.Initialise("127.0.0.1", 2024)) {
		std::cout << "init error" << std::endl;
		return;
	}

	std::cout << +client.GetMyAddress() << std::endl;
	client.SendTCP(client.Broadcast, "HELLO, WORLD", strlen("HELLO, WORLD"), 10);
	client.SendUDP(client.Broadcast, "HELLO, WORLD", strlen("HELLO, WORLD"), 10, 4);

	client.StartUDPReceiveServer(callback_udp);

	CClient::ClientProtocolProcessor<SOCKET, INVALID_SOCKET, SOCKET_ERROR, sockaddr>::DistributionType d_type;  
	char addr, data4bits, *buffer = new char[2]; 
	unsigned short bufflen = 2, max_bufflen = bufflen;

	while (client.ReceiveTCP(d_type, addr, data4bits, buffer, bufflen))
	{
		std::cout << "received tcp: " << bufflen << " first char: " << *buffer << std::endl;

		if (bufflen > max_bufflen)
			max_bufflen = bufflen;
		else
			bufflen = max_bufflen;
	}

	client.Finalise();

	WSACleanup();
}
