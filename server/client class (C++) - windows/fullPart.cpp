#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include "fullPart.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")


using namespace std;

bool ClientProtocolProcessor::Initialise(const char* ipAddress, unsigned short port, bool useUDP) {
	if (initialised) throw exception("Already initialized");
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) { WSACleanup(); throw exception("Can not run winsocks");  }
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ipAddress, &addr.sin_addr.S_un.S_addr) != 1) { WSACleanup(); throw exception("Error on transforming 'const char*' to 'IP'");  }
	TCPConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (TCPConnectSocket == INVALID_SOCKET) { WSACleanup(); throw exception("Can not create TCP socket");  }
	if (connect(TCPConnectSocket, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) { closesocket(TCPConnectSocket); WSACleanup(); return false; }
	int temp = 1; char addrbuf[6];
	temp = recv(TCPConnectSocket, addrbuf, 6, 0);
	if (temp != 6) {
		shutdown(TCPConnectSocket, SD_BOTH);
		closesocket(TCPConnectSocket);
		WSACleanup();
		return false;
	}
	address = addrbuf[5];
	if (use_udp = useUDP) {
		UDPConnectSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (UDPConnectSocket == INVALID_SOCKET) { shutdown(TCPConnectSocket, SD_BOTH); closesocket(TCPConnectSocket); WSACleanup(); throw exception("Can not create UDP socket"); }
		ZeroMemory(&addr, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		inet_pton(AF_INET, ipAddress, &addr.sin_addr.s_addr);
		UDPsendaddress = *(sockaddr*)&addr;
		udp_buffer[7] = udp_buffer[6] = udp_buffer[5] = udp_buffer[4] = udp_buffer[8] = *udp_buffer = tcp_buffer[9] = tcp_buffer[1] = udp_buffer[2] = 0;
		udp_buffer[3] = 4;
		udp_buffer[10] = address;
		sendto(UDPConnectSocket, udp_buffer, 11, 0, &UDPsendaddress, sizeof(UDPsendaddress));
	}
	setsockopt(TCPConnectSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&temp, sizeof(temp));
	tcp_buffer = new char[128];
	udp_buffer = new char[1432];
	initialised = true;
	return true;
}
void ClientProtocolProcessor::SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address) {
	if (!distr) {
		if (buffersize + 5 > current_tcp_bufflen) {
			delete[] tcp_buffer;
			current_tcp_bufflen = (buffersize + 5 >> 7) + 1 << 7;
			tcp_buffer = new char[current_tcp_bufflen];
		}
		tcp_buffer[3] = *tcp_buffer = buffersize & 0xFF;
		tcp_buffer[4] = tcp_buffer[1] = buffersize >> 8;
		tcp_buffer[2] = 0 | userdata4bits << 4;
		memcpy(tcp_buffer + 5, buffer, buffersize);
		send(TCPConnectSocket, tcp_buffer, buffersize + 5, 0);
	} else {
		++buffersize;
		if (buffersize + 5 > current_tcp_bufflen) {
			delete[] tcp_buffer;
			current_tcp_bufflen = (buffersize + 5 >> 7) + 1 << 7;
			tcp_buffer = new char[current_tcp_bufflen];
		}
		tcp_buffer[3] = *tcp_buffer = buffersize & 0xFF;
		tcp_buffer[4] = tcp_buffer[1] = buffersize >> 8;
		tcp_buffer[2] = 2 | userdata4bits << 4;
		tcp_buffer[5] = address;
		memcpy(tcp_buffer + 6, buffer, buffersize-1);
		send(TCPConnectSocket, tcp_buffer, buffersize + 5, 0);
	}
}

void ClientProtocolProcessor::SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting, char unicast_address) {
	if (buffersize > 1421) throw exception("Overflow");
	udp_buffer[8] = *udp_buffer = buffersize & 0xFF;
	tcp_buffer[9] = tcp_buffer[1] = buffersize >> 8 & 0xFF;
	tcp_buffer[2] = udp_mainTypeUserdata;
	if (packet_GroupingCounting) {
		if (udp_counter == 4294967295) {
			udp_counter = 0;
			tcp_buffer[4] = tcp_buffer[1] = tcp_buffer[3] = *tcp_buffer = 0;
			tcp_buffer[2] = 4;
			send(TCPConnectSocket, tcp_buffer, 5, 0);
		}
		tcp_buffer[4] = udp_counter & 0xFF;
		tcp_buffer[5] = udp_counter >> 8 & 0xFF;
		tcp_buffer[6] = udp_counter >> 8 & 0xFF;
		tcp_buffer[7] = udp_counter >> 8;
	}
	
	tcp_buffer[10] = address;
	if (!distr) {
		tcp_buffer[3] = 0 | packet_GroupingCounting | userdata4bits << 4;
		memcpy(tcp_buffer + 5, buffer, buffersize);
		send(TCPConnectSocket, tcp_buffer, buffersize + 5, 0);
	} else {
		tcp_buffer[3] = 2 | packet_GroupingCounting | userdata4bits << 4;
		memcpy(tcp_buffer + 6, buffer, buffersize - 1);
		send(TCPConnectSocket, tcp_buffer, buffersize + 5, 0);
	}
}


void ClientProtocolProcessor::Finalise() {
	if (!initialised) throw exception("Uninitialized");
	shutdown(TCPConnectSocket, SD_BOTH);
	closesocket(TCPConnectSocket);
	if (use_udp) closesocket(UDPConnectSocket);
	delete[] tcp_buffer;
	delete[] udp_buffer;
	WSACleanup();
}

char ClientProtocolProcessor::GetMyAddress() {
	if (!initialised) throw exception("Uninitialized");
	return address;
}
