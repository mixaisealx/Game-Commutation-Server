#pragma once
//#include <winsock2.h>
#include <mutex>
#include <vector>
#include <unordered_map>

namespace CClient {
	bool InitialeseGlobalWSA();
	void CleanupGlobalWSA();
	//template <typename socket_type, socket_type uncorrect_socket>
	class ClientProtocolProcessorException : public std::exception {
	private:
		std::string message_;
	public:
		explicit ClientProtocolProcessorException(const std::string& message) :message_(message) {};
		virtual const char* what() const throw() {
			return message_.c_str();
		}
	};
	class ClientProtocolProcessor {
		SOCKET TCPConnectSocket = INVALID_SOCKET, UDPConnectSocket = INVALID_SOCKET;
		bool uninitialised = true, nuse_udp = true, udp_listening = false;
		char *tcp_buffer, tempaddrbuff; unsigned short current_tcp_bufflen = 128, temp, udp_temp;
		char *udp_buffer, *udp_buffer_receive;
		char header_tcp_buffer[5], tcp_special_buffer[6];
		unsigned char address; sockaddr UDPsendaddress;
		unsigned udp_counter = 0, udp_rec_counter = 0;
		std::mutex mutex_tcp_sender, mutex_tcp_receiver, mutex_udp_sender, mutex_tcp_special_sender, thread_release, mutex_udp_server_proc;
		ClientProtocolProcessor(const ClientProtocolProcessor&);
		ClientProtocolProcessor operator=(const ClientProtocolProcessor&) { };
		std::thread udp_server_thread, udp_server_rec_thread; bool thread_ncancelled, thread_released[2];
	public:
		enum DistributionType { Broadcast, Unicast, ReceiveSpecilalTCP_Indexes, ReceiveSpecilalTCP_State };
		ClientProtocolProcessor() { }
#pragma warning (disable: 4297)
		~ClientProtocolProcessor() { if (udp_listening) throw ClientProtocolProcessorException("Incorrect UDP server shutdown"); }
#pragma warning (default: 4297)
		bool Initialise(const char* ipAddress, unsigned short port, bool useUDP = true);
		//Calls allowed only from ONE thread
		void SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address = NULL);
		//Calls allowed only from ONE thread
		void SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting = true, char unicast_address = NULL);
		//Calls allowed only from ONE thread
		//"current_bufflen" - length of data on return OR max size of "buffer" on the function call
		//You must store the maximum buffer size in a separate variable. The function itself allocates memory, so in the length of the message it can send a new maximum buffer size. 
		//A new size is given only if the start file does not contain the desired amount of bytes.
		bool ReceiveTCP(DistributionType &distr, char &unicast_address, char &userdatd4bits, char *&buffer, unsigned short &current_bufflen);
		//Calls allowed only from ONE thread
		void SendTCPSpecial(DistributionType specialReceiveType, char address = NULL);
		//Calls allowed only from ONE thread
		//"current_bufflen" - length of data on return
		//Size of buffer must be 1421 byte (more is possible, less is not).
		void ReceiveEveryUDP(DistributionType &distr, char &udp_mainTypeUserdata, char &userdatd4bits, char *buffer, unsigned short &datalen, char &unicast_address);
		typedef void (*UDPReceiverCallback)(DistributionType distr, char udp_mainTypeUserdata, char userdatd4bits, char *buffer, unsigned short datalen, char unicast_address);
		bool StartUDPReceiveServer(UDPReceiverCallback callback);
		bool StopUDPReceiveServer();
		char GetMyAddress();
		void Finalise();
	private:
		struct UDPacketStand { UDPacketStand() {} UDPacketStand(const char type, const char addr, const char control, char *buffer, const unsigned short bufflen) :type(type), addr(addr), control(control), buffer(buffer), bufflen(bufflen) {} char type, addr, control, *buffer; unsigned short bufflen; };
		struct UDPacketTimed { UDPacketTimed(char addr, char control, char *buffer, unsigned short bufflen):addr(addr), control(control), buffer(buffer), bufflen(bufflen) {} char addr, control, *buffer; unsigned short bufflen; };
		std::vector<UDPacketStand> udp_queue_stand; std::vector<UDPacketStand>::iterator udp_start_stand, udp_end_stand;
		std::unordered_map<char, UDPacketTimed> udp_queue_timed; std::unordered_map<char, UDPacketTimed>::iterator udp_end_timed;
		void UdpServerThread(UDPReceiverCallback callback);
		void UdpServerRecUDPThread();
	};
	
}