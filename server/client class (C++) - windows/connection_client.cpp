#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <exception>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#include "connection_client.h"

using namespace std;

bool ClientProtocolProcessor::Initialise(const char* ipAddress, unsigned short port, bool useUDP) {
	if (initialised) throw exception("Already initialized");
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) { WSACleanup(); throw exception("Can not run winsocks"); }
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ipAddress, &addr.sin_addr.S_un.S_addr) != 1) { WSACleanup(); throw exception("Error on transforming 'const char*' to 'IP'"); }
	TCPConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (TCPConnectSocket == INVALID_SOCKET) { WSACleanup(); throw exception("Can not create TCP socket"); }
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
		inet_pton(AF_INET, ipAddress, &addr.sin_addr.s_addr);
		UDPsendaddress = *(sockaddr*)&addr;
		udp_buffer = new char[1432];
		udp_buffer_receive = new char[1432];
		udp_buffer[7] = udp_buffer[6] = udp_buffer[5] = udp_buffer[4] = udp_buffer[8] = *udp_buffer = udp_buffer[9] = udp_buffer[1] = udp_buffer[2] = 0;
		udp_buffer[3] = 4;
		udp_buffer[10] = address;
		sendto(UDPConnectSocket, udp_buffer, 11, 0, &UDPsendaddress, sizeof(UDPsendaddress));
		
	}
	tcp_buffer = new char[128];
	initialised = true;
	return true;
}
void ClientProtocolProcessor::SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address) {
	if (!initialised) throw exception("Uninitialized");
	if (!distr) {
		if (buffersize + 5 > current_tcp_bufflen) {
			delete[] tcp_buffer;
			current_tcp_bufflen = buffersize + 5;
			tcp_buffer = new char[current_tcp_bufflen];
		}
		tcp_buffer[3] = *tcp_buffer = buffersize & 0xFF;
		tcp_buffer[4] = tcp_buffer[1] = buffersize >> 8;
		tcp_buffer[2] = 0 | userdata4bits << 4;
		memcpy(tcp_buffer + 5, buffer, buffersize);
		send(TCPConnectSocket, tcp_buffer, buffersize + 5, 0);
	} else {
		if (buffersize++ + 5 > current_tcp_bufflen) {
			delete[] tcp_buffer;
			current_tcp_bufflen = buffersize + 5;
			tcp_buffer = new char[current_tcp_bufflen];
		}
		tcp_buffer[3] = *tcp_buffer = buffersize & 0xFF;
		tcp_buffer[4] = tcp_buffer[1] = buffersize >> 8;
		tcp_buffer[2] = 2 | userdata4bits << 4;
		tcp_buffer[5] = unicast_address;
		memcpy(tcp_buffer + 6, buffer, buffersize - 1);
		send(TCPConnectSocket, tcp_buffer, buffersize + 5, 0);
	}
}

void ClientProtocolProcessor::SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting, char unicast_address) {
	if (!initialised) throw exception("Uninitialized");
	if (buffersize > 1421) throw exception("Overflow");
	if (packet_GroupingCounting) {
		if (udp_counter != 4294967295) ++udp_counter;
		else {
			udp_counter = 0;
			udp_buffer[4] = udp_buffer[1] = udp_buffer[3] = *udp_buffer = 0;
			udp_buffer[2] = 4;
			send(TCPConnectSocket, udp_buffer, 5, 0);
		}
		udp_buffer[4] = udp_counter & 0xFF;
		udp_buffer[5] = udp_counter >> 8 & 0xFF;
		udp_buffer[6] = udp_counter >> 8 & 0xFF;
		udp_buffer[7] = udp_counter >> 8;
	}
	udp_buffer[8] = *udp_buffer = buffersize & 0xFF;
	udp_buffer[9] = udp_buffer[1] = buffersize >> 8 & 0xFF;
	udp_buffer[2] = udp_mainTypeUserdata;
	if (!distr) {
		udp_buffer[10] = address;
		udp_buffer[3] = 0 | packet_GroupingCounting | userdata4bits << 4;
	} else {
		udp_buffer[10] = unicast_address;
		udp_buffer[3] = 2 | packet_GroupingCounting | userdata4bits << 4;
	}
	memcpy(udp_buffer + 11, buffer, buffersize);
	sendto(UDPConnectSocket, udp_buffer, buffersize + 11, 0, &UDPsendaddress, sizeof(UDPsendaddress));
}

bool ClientProtocolProcessor::ReceiveTCP(DistributionType &distr, char &unicast_address, char &userdatd4bits, char **buffer, unsigned short &current_bufflen)
{
	if (!initialised) throw exception("Uninitialized");
	systemexec:
	if (recv(TCPConnectSocket, header_tcp_buffer, 5, 0) == 5) {
		if (*header_tcp_buffer == header_tcp_buffer[3] && header_tcp_buffer[1] == header_tcp_buffer[4]) {
			switch (header_tcp_buffer[2] & 0xF)
			{
			case 0:
				if ((temp = *header_tcp_buffer | header_tcp_buffer[1] << 8) > current_bufflen) {
					delete[] *buffer;
					current_bufflen = temp;
					*buffer = new char[temp];
				} else current_bufflen = temp;
				distr = DistributionType::Broadcast;
				userdatd4bits = (header_tcp_buffer[2] & 0xF0) >> 4;
				recv(TCPConnectSocket, *buffer, temp, 0);
				break;
			case 2:
				if ((temp = (*header_tcp_buffer | header_tcp_buffer[1] << 8) - 1) > current_bufflen) {
					delete[] *buffer;
					current_bufflen = temp;
					*buffer = new char[temp];
				} else current_bufflen = temp;
				distr = DistributionType::Unicast;
				userdatd4bits = (header_tcp_buffer[2] & 0xF0) >> 4;
				recv(TCPConnectSocket, &tempaddrbuff, 1, 0);
				unicast_address = tempaddrbuff;
				recv(TCPConnectSocket, *buffer, temp, 0);
				break;
			case 3:
				distr = DistributionType::ReceiveSpecilalTCP_State;
				if (current_bufflen == 0) {
					delete[] *buffer;
					current_bufflen = 1;
					*buffer = new char[1];
				} else current_bufflen = 0;
				recv(TCPConnectSocket, *buffer, 1, 0);
				break;
			case 1:
				distr = DistributionType::ReceiveSpecilalTCP_Indexes;
				if ((temp = *header_tcp_buffer | header_tcp_buffer[1] << 8) > current_bufflen) {
					delete[] *buffer;
					current_bufflen = temp;
					*buffer = new char[temp];
				} else  current_bufflen = temp;
				recv(TCPConnectSocket, *buffer, temp, 0);
				break;
			case 4:
				udp_counter = 0;
				goto systemexec;
				break;
			}
			return true;
		} else {
			Finalise();
			throw exception("Uncorrect packet");
		}
	} else {
		Finalise();
		return false;
	}
}

void ClientProtocolProcessor::SendTCPSpecial(DistributionType specialReceiveType, char address) {
	if (!initialised) throw exception("Uninitialized");
	if (specialReceiveType != DistributionType::ReceiveSpecilalTCP_Indexes) {
		*tcp_special_buffer = tcp_special_buffer[3] = 1;
		tcp_special_buffer[1] = tcp_special_buffer[4] = 0;
		tcp_special_buffer[2] = 3;
		tcp_special_buffer[5] = address;
		send(TCPConnectSocket, tcp_special_buffer, 6, 0);
	} else {
		*tcp_special_buffer = tcp_special_buffer[3] = tcp_special_buffer[1] = tcp_special_buffer[4] = 0;
		tcp_special_buffer[2] = 1;
		send(TCPConnectSocket, tcp_special_buffer, 5, 0);
	}
}

void ClientProtocolProcessor::Finalise() {
	if (!initialised) throw exception("Uninitialized");
	shutdown(TCPConnectSocket, SD_BOTH);
	closesocket(TCPConnectSocket);
	if (use_udp) closesocket(UDPConnectSocket);
	delete[] tcp_buffer;
	delete[] udp_buffer;
	delete[] udp_buffer_receive;
	WSACleanup();
	initialised = false;
}

char ClientProtocolProcessor::GetMyAddress() {
	if (!initialised) throw exception("Uninitialized");
	return address;
}