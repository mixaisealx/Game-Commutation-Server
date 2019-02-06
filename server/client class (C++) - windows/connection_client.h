#pragma once
//#include <winsock2.h>

class ClientProtocolProcessor {
	SOCKET TCPConnectSocket = INVALID_SOCKET, UDPConnectSocket = INVALID_SOCKET;
	bool initialised = false, use_udp = true;
	char *tcp_buffer, tempaddrbuff; unsigned short current_tcp_bufflen = 128, temp;
	char *udp_buffer, *udp_buffer_receive;
	char header_tcp_buffer[5], tcp_special_buffer[6];
	unsigned char address; sockaddr UDPsendaddress, socktemp;
	unsigned udp_counter = 0;
	ClientProtocolProcessor(const ClientProtocolProcessor&);
	ClientProtocolProcessor operator=(const ClientProtocolProcessor&) { };
public:
	enum DistributionType { Broadcast, Unicast, ReceiveSpecilalTCP_Indexes, ReceiveSpecilalTCP_State };
	ClientProtocolProcessor() { }

	bool Initialise(const char* ipAddress, unsigned short port, bool useUDP = true);
	//Calls allowed only from ONE thread
	void SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address = NULL);
	//Calls allowed only from ONE thread
	void SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting = true, char unicast_address = NULL);
	//Calls allowed only from ONE thread
	//"current_bufflen" - length of data on return OR max size of "buffer" on the function call
	//You must store the maximum buffer size in a separate variable. The function itself allocates memory, so in the length of the message it can send a new maximum buffer size. 
	//A new size is given only if the start file does not contain the desired amount of bytes.
	bool ReceiveTCP(DistributionType &distr, char &unicast_address, char &userdatd4bits, char **buffer, unsigned short &current_bufflen);
	//Calls allowed only from ONE thread
	void SendTCPSpecial(DistributionType specialReceiveType, char address = NULL);

	char GetMyAddress();
	void Finalise();
};