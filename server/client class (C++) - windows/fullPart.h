#pragma once

class ClientProtocolProcessor {
	SOCKET TCPConnectSocket = INVALID_SOCKET, UDPConnectSocket = INVALID_SOCKET;
	bool initialised = false, use_udp = true;
	char *tcp_buffer; unsigned short current_tcp_bufflen;
	char *udp_buffer;
	unsigned char address; sockaddr UDPsendaddress, socktemp;
	unsigned udp_counter = 0;
	ClientProtocolProcessor(const ClientProtocolProcessor&);
	ClientProtocolProcessor operator=(const ClientProtocolProcessor&) { };
public:
	enum DistributionType { Broadcast, Unicast };
	ClientProtocolProcessor() { }

	bool Initialise(const char* ipAddress, unsigned short port, bool useUDP = true);
	void SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address = NULL);
	void SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting = true, char unicast_address = NULL);
	char GetMyAddress();
	void Finalise();
};