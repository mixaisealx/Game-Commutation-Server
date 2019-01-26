#pragma once
#include <winsock2.h>

class ClientProtocolProcessor {
	SOCKET TCPConnectSocket = INVALID_SOCKET, UDPConnectSocket = INVALID_SOCKET;
	bool initialised = false, use_udp = true;
	char *tcp_buffer, tempaddrbuff; unsigned short current_tcp_bufflen, temp;
	char *udp_buffer;
	char header_tcp_buffer[5], header_udp_buffer[11];
	unsigned char address; sockaddr UDPsendaddress, socktemp;
	unsigned udp_counter = 0;
	ClientProtocolProcessor(const ClientProtocolProcessor&);
	ClientProtocolProcessor operator=(const ClientProtocolProcessor&) { };
public:
	enum DistributionType { Broadcast, Unicast };
	ClientProtocolProcessor() { }

	bool Initialise(const char* ipAddress, unsigned short port, bool useUDP = true);
	//Send calls allowed only from ONE thread
	void SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address = NULL);
	//Send calls allowed only from ONE thread
	void SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting = true, char unicast_address = NULL);
	void ReceiveTCP(DistributionType &distr, char &unicast_address, char &userdatd4bits, char *buffer, unsigned short &current_bufflen);
	char GetMyAddress();
	void Finalise();
};