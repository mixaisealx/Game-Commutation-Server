#pragma once
#include <mutex>
#include <vector>
#include <unordered_map>
#include <exception>
#include <chrono>
#include <thread>

namespace CClient {
	class ClientProtocolProcessorException : public std::exception {
	private:
		std::string message_;
	public:
		explicit ClientProtocolProcessorException(const std::string& message) :message_(message) {};
		virtual const char* what() const throw() {
			return message_.c_str();
		}
	};
	//Must contain links to independent functions only
	template<typename socket_t, typename address_t>
	struct ClientProtocolProcessorFunctions {
		using FuncPrepareSocketToConnection = bool(*) (address_t &address, const char* text_addr, unsigned short port);
		using FuncConnectSocketTCP = bool(*) (socket_t &socket, address_t &address);
		using FuncEnableKeepAliveTCP = bool(*) (socket_t &socket);

		using FuncCreateStreamTCPsocket = void(*) (socket_t &socket);
		using FuncCreateUDPsocket = void(*) (socket_t &socket);
		using FuncCloseSocket = void(*) (socket_t &socket);
		using FuncShutdownTCPSocketBoth = void(*) (socket_t &socket);
		using FuncSendTCP = void(*) (socket_t &socket, const char *buffer, int buffersize);
		using FuncSendtoUDP = void(*) (socket_t &socket, const char *buffer, int buffersize, const address_t &address);

		using FuncReceiveTCP = int(*) (socket_t &socket, char *buffer, int buffersize);
		using FuncReceivefromUDP = int(*) (socket_t &socket, char *buffer, int buffersize);

		FuncPrepareSocketToConnection prepare_conn;
		FuncConnectSocketTCP conn_tcp;
		FuncCreateStreamTCPsocket create_tcp;
		FuncCreateUDPsocket create_udp;
		FuncCloseSocket close;
		FuncShutdownTCPSocketBoth shutdown_tcp;
		FuncSendTCP send_tcp;
		FuncSendtoUDP send_udp;
		FuncReceiveTCP recv_tcp;
		FuncReceivefromUDP recv_udp;
		FuncEnableKeepAliveTCP enable_keepalive_tcp;

		ClientProtocolProcessorFunctions() {}
		ClientProtocolProcessorFunctions(FuncPrepareSocketToConnection prepare_conn, FuncCreateStreamTCPsocket create_tcp, FuncConnectSocketTCP conn_tcp, FuncShutdownTCPSocketBoth shutdown_tcp, 
			FuncCreateUDPsocket create_udp, FuncCloseSocket close, FuncSendTCP send_tcp, FuncReceiveTCP recv_tcp, FuncSendtoUDP send_udp, FuncReceivefromUDP recv_udp, FuncEnableKeepAliveTCP enable_keepalive_tcp):
			prepare_conn(prepare_conn), create_tcp(create_tcp), conn_tcp(conn_tcp), shutdown_tcp(shutdown_tcp), create_udp(create_udp), close(close), 
			send_tcp(send_tcp), recv_tcp(recv_tcp), send_udp(send_udp), recv_udp(recv_udp), enable_keepalive_tcp(enable_keepalive_tcp){}
	};

	template<typename socket_t, socket_t invalid_variant, int socket_error, typename address_t>
	class ClientProtocolProcessor {
		socket_t TCPConnectSocket = invalid_variant, UDPConnectSocket = invalid_variant;
		bool uninitialised = true, nuse_udp = true, udp_listening = false;
		char *tcp_buffer, tempaddrbuff; unsigned short current_tcp_bufflen = 128, temp, udp_temp;
		char *udp_buffer, *udp_buffer_receive;
		char header_tcp_buffer[5], tcp_special_buffer[6];
		unsigned char address; address_t UDPsendaddress;
		unsigned udp_counter = 0, udp_rec_counter = 0;
		std::mutex mutex_tcp_sender, mutex_tcp_receiver, mutex_udp_sender, mutex_tcp_special_sender, mutex_udp_server_proc;
		ClientProtocolProcessor(const ClientProtocolProcessor&);
		ClientProtocolProcessor operator=(const ClientProtocolProcessor&);
		std::thread udp_server_thread, udp_server_rec_thread; bool thread_ncancelled, thread_released[2];

		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncCloseSocket sock_close;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncConnectSocketTCP sock_conn_tcp;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncShutdownTCPSocketBoth sock_shutdown_tcp;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncPrepareSocketToConnection sock_prepare_conn;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncEnableKeepAliveTCP sock_enable_keepalive_tcp;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncSendTCP sock_send_tcp; typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncReceiveTCP sock_recv_tcp;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncSendtoUDP sock_send_udp; typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncReceivefromUDP sock_recv_udp;
		typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncCreateStreamTCPsocket sock_create_tcp; typename ClientProtocolProcessorFunctions<socket_t, address_t>::FuncCreateUDPsocket sock_create_udp;

		inline bool func_tcp_receive(char *buffer, int bytes_count) {
			int rdbytes = sock_recv_tcp(TCPConnectSocket, buffer, bytes_count);
			if (rdbytes <= 0) return false;
			while ((bytes_count -= rdbytes) != 0) {
				buffer += rdbytes;
				if ((rdbytes = sock_recv_tcp(TCPConnectSocket, buffer, bytes_count)) <= 0) return false;
			}
			return true;
		}
	public:
		enum DistributionType { Broadcast, Unicast, ReceiveSpecilalTCP_Indexes, ReceiveSpecilalTCP_State };
		explicit ClientProtocolProcessor(ClientProtocolProcessorFunctions<socket_t, address_t> funcs) {
			sock_close = funcs.close;
			sock_conn_tcp = funcs.conn_tcp;
			sock_shutdown_tcp = funcs.shutdown_tcp;
			sock_prepare_conn = funcs.prepare_conn;
			sock_enable_keepalive_tcp = funcs.enable_keepalive_tcp;
			sock_send_tcp = funcs.send_tcp; sock_recv_tcp = funcs.recv_tcp;
			sock_send_udp = funcs.send_udp; sock_recv_udp = funcs.recv_udp;
			sock_create_tcp = funcs.create_tcp; sock_create_udp = funcs.create_udp;
		}
#pragma warning (disable: 4297)
		~ClientProtocolProcessor() { if (udp_listening) throw ClientProtocolProcessorException("Incorrect UDP server shutdown"); }
#pragma warning (default: 4297)
		//Perform client initialization.
		bool Initialise(const char* ipAddress, unsigned short port, bool useUDP = true) {
			if (!uninitialised) throw ClientProtocolProcessorException("Already initialized");
			address_t addrt;
			if (!sock_prepare_conn(addrt, ipAddress, port)) { throw ClientProtocolProcessorException("Error on transforming 'const char*' to 'IP'"); }
			sock_create_tcp(TCPConnectSocket);
			if (TCPConnectSocket == invalid_variant) { throw ClientProtocolProcessorException("Can not create TCP socket"); }
			if (!(sock_conn_tcp(TCPConnectSocket, addrt) && sock_enable_keepalive_tcp(TCPConnectSocket))) { sock_close(TCPConnectSocket); return false; }
			char addrbuf[6];
			if (!func_tcp_receive(addrbuf, 6)) {
				sock_shutdown_tcp(TCPConnectSocket);
				sock_close(TCPConnectSocket);
				return false;
			}
			address = addrbuf[5];
			nuse_udp = !useUDP;
			if (useUDP) {
				sock_create_udp(UDPConnectSocket);
				if (UDPConnectSocket == invalid_variant) { sock_shutdown_tcp(TCPConnectSocket); sock_close(TCPConnectSocket); throw ClientProtocolProcessorException("Can not create UDP socket"); }
				sock_prepare_conn(addrt, ipAddress, port);
				UDPsendaddress = addrt;
				udp_buffer = new char[1432];
				udp_buffer_receive = new char[1432];
				udp_buffer[7] = udp_buffer[6] = udp_buffer[5] = udp_buffer[4] = udp_buffer[8] = *udp_buffer = udp_buffer[9] = udp_buffer[1] = udp_buffer[2] = 0;
				udp_buffer[3] = 4;
				udp_buffer[10] = address;
				sock_send_udp(UDPConnectSocket, udp_buffer, 11, UDPsendaddress);
			}
			tcp_buffer = new char[128];
			uninitialised = false;
			return true;
		}
		//Calls allowed only from ONE thread
		void SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address = NULL) {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			mutex_tcp_sender.lock();
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
				sock_send_tcp(TCPConnectSocket, tcp_buffer, buffersize + 5);
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
				sock_send_tcp(TCPConnectSocket, tcp_buffer, buffersize + 5);
			}
			mutex_tcp_sender.unlock();
		}
		//Calls allowed only from ONE thread
		void SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting = true, char unicast_address = NULL) {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
			if (buffersize > 1421) throw ClientProtocolProcessorException("Overflow");
			mutex_udp_sender.lock();
			if (packet_GroupingCounting) {
				if (udp_counter != 4294967295) ++udp_counter;
				else {
					udp_counter = 0;
					udp_buffer[4] = udp_buffer[1] = udp_buffer[3] = *udp_buffer = 0;
					udp_buffer[2] = 4;
					sock_send_tcp(TCPConnectSocket, udp_buffer, 5);
				}
				udp_buffer[4] = udp_counter & 0xFF;
				udp_buffer[5] = udp_counter >> 8 & 0xFF;
				udp_buffer[6] = udp_counter >> 8 & 0xFF;
				udp_buffer[7] = udp_counter >> 8;
			}
			udp_buffer[8] = *udp_buffer = buffersize & 0xFF;
			udp_buffer[9] = udp_buffer[1] = buffersize >> 8 & 0xFF;
			udp_buffer[2] = udp_mainTypeUserdata;
#pragma warning (disable: 4806)
			if (!distr) {
				udp_buffer[10] = address;
				udp_buffer[3] = 0 | packet_GroupingCounting | userdata4bits << 4;
			} else {
				udp_buffer[10] = unicast_address;
				udp_buffer[3] = 2 | packet_GroupingCounting | userdata4bits << 4;
			}
#pragma warning (default: 4806)
			memcpy(udp_buffer + 11, buffer, buffersize);
			sock_send_udp(UDPConnectSocket, udp_buffer, buffersize + 11, UDPsendaddress);
			mutex_udp_sender.unlock();
		}
		//Calls allowed only from ONE thread
		//"current_bufflen" - length of data on return OR max size of "buffer" on the function call
		//You must store the maximum buffer size in a separate variable. The function itself allocates memory, so in the length of the message it can send a new maximum buffer size. 
		//A new size is given only if the start file does not contain the desired amount of bytes.
		bool ReceiveTCP(DistributionType &distr, char &unicast_address, char &userdatd4bits, char *&buffer, unsigned short &current_bufflen) {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			mutex_tcp_receiver.lock();
		systemexec:
			if (func_tcp_receive(header_tcp_buffer, 5)) {
				if (*header_tcp_buffer == header_tcp_buffer[3] && header_tcp_buffer[1] == header_tcp_buffer[4]) {
					switch (header_tcp_buffer[2] & 0xF)
					{
					case 0:
						if ((temp = *header_tcp_buffer | header_tcp_buffer[1] << 8) > current_bufflen) {
							delete[] buffer;
							current_bufflen = temp;
							buffer = new char[temp];
						} else current_bufflen = temp;
						distr = DistributionType::Broadcast;
						userdatd4bits = (header_tcp_buffer[2] & 0xF0) >> 4;
						if (temp && !func_tcp_receive(buffer, temp)) goto error;
						break;
					case 2:
						if ((temp = (*header_tcp_buffer | header_tcp_buffer[1] << 8) - 1) > current_bufflen) {
							delete[] buffer;
							current_bufflen = temp;
							buffer = new char[temp];
						} else current_bufflen = temp;
						distr = DistributionType::Unicast;
						userdatd4bits = (header_tcp_buffer[2] & 0xF0) >> 4;
						if (sock_recv_tcp(TCPConnectSocket, &tempaddrbuff, 1) != 1) goto error;
						unicast_address = tempaddrbuff;
						if (temp && !func_tcp_receive(buffer, temp)) goto error;
						break;
					case 3:
						distr = DistributionType::ReceiveSpecilalTCP_State;
						if (current_bufflen == 0) {
							delete[] buffer;
							current_bufflen = 1;
							buffer = new char[1];
						} else current_bufflen = 0;
						if (!func_tcp_receive(buffer, 1)) goto error;
						break;
					case 1:
						distr = DistributionType::ReceiveSpecilalTCP_Indexes;
						if ((temp = *header_tcp_buffer | header_tcp_buffer[1] << 8) > current_bufflen) {
							delete[] buffer;
							current_bufflen = temp;
							buffer = new char[temp];
						} else  current_bufflen = temp;
						if (temp && !func_tcp_receive(buffer, temp)) goto error;
						break;
					case 4:
						udp_rec_counter = 0;
						goto systemexec;
						break;
					}
					mutex_tcp_receiver.unlock();
					return true;
				} else {
				error:
					mutex_tcp_receiver.unlock();
					Finalise();
					return false;
				}
			} else {
				mutex_tcp_receiver.unlock();
				Finalise();
				return false;
			}
		}
		//Calls allowed only from ONE thread
		void SendTCPSpecial(DistributionType specialReceiveType, char address = NULL) {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			mutex_tcp_special_sender.lock();
			if (specialReceiveType != DistributionType::ReceiveSpecilalTCP_Indexes) {
				*tcp_special_buffer = tcp_special_buffer[3] = 1;
				tcp_special_buffer[1] = tcp_special_buffer[4] = 0;
				tcp_special_buffer[2] = 3;
				tcp_special_buffer[5] = address;
				sock_send_tcp(TCPConnectSocket, tcp_special_buffer, 6);
			} else {
				*tcp_special_buffer = tcp_special_buffer[3] = tcp_special_buffer[1] = tcp_special_buffer[4] = 0;
				tcp_special_buffer[2] = 1;
				sock_send_tcp(TCPConnectSocket, tcp_special_buffer, 5);
			}
			mutex_tcp_special_sender.unlock();
		}
		//Calls allowed only from ONE thread
		//"current_bufflen" - length of data on return
		//Size of buffer must be 1421 byte (more is possible, less is not).
		void ReceiveEveryUDP(DistributionType &distr, char &udp_mainTypeUserdata, char &userdatd4bits, char *buffer, unsigned short &datalen, char &broadcast_sender_address) {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
			mutex_udp_server_proc.lock();
			if (udp_listening) {
				mutex_udp_server_proc.unlock();
				throw ClientProtocolProcessorException("UDP already listening");
			}
			udp_listening = true;
		systemexec:
			if ((datalen = sock_recv_udp(UDPConnectSocket, udp_buffer_receive, 1432)) != socket_error) {
				if (*udp_buffer_receive != udp_buffer_receive[8] || udp_buffer_receive[1] != udp_buffer_receive[9]) goto systemexec;
				if (udp_buffer_receive[3] & 0xC) goto systemexec;
				datalen -= 11;
				userdatd4bits = (udp_buffer_receive[3] & 0xF0) >> 4;
				udp_mainTypeUserdata = udp_buffer_receive[2];
				if (udp_buffer_receive[3] & 0x2) {
					distr = DistributionType::Unicast;
				} else {
					distr = DistributionType::Broadcast;
					broadcast_sender_address = udp_buffer_receive[10];
				}
				memcpy(buffer, udp_buffer_receive + 11, datalen);
				udp_listening = false;
			} else 
				goto systemexec;
			mutex_udp_server_proc.unlock();
		}
		using UDPReceiverCallback = void(*) (DistributionType distr, char udp_mainTypeUserdata, char userdatd4bits, char *buffer, unsigned short datalen, char broadcast_sender_address);
		typename UDPReceiverCallback udp_receiver_callback;
		//Calls for UDP RECEIVE allowed only from ONE thread
		//Start listening UDP in this class. This listening server implements all protocol functions (regarding UDP) independently.
		//In the standard situation in this server is not necessary. However, it can be useful if you only need to accept the latest UDP (the old ones so that they are not ahead of the new ones). However, taking into account the delay of 50ms, there is a considerable likelihood of some ordering of packet arrival.
		bool StartUDPReceiveServer(UDPReceiverCallback callback) {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
			mutex_udp_server_proc.lock();
			if (udp_listening) {
				mutex_udp_server_proc.unlock();
				return false;
			}
			udp_listening = thread_ncancelled = true;
			mutex_udp_server_proc.unlock();
			thread_released[0] = thread_released[1] = false;
			udp_end_stand = udp_start_stand = udp_queue_stand.end();
			udp_end_timed = udp_queue_timed.end();
			udp_receiver_callback = callback;
			udp_server_thread = std::thread([&]() { UdpServerThread(); });
			udp_server_rec_thread = std::thread([&]() { UdpServerRecUDPThread(); });
			return true;
		}
		//Stop listening UDP in this class.
		bool StopUDPReceiveServer() {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
			if (!thread_ncancelled) return false;
			thread_ncancelled = false;
			return true;
		}

		char GetMyAddress() {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			return address;
		}

		void Finalise() {
			if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
			if (udp_listening) throw ClientProtocolProcessorException("UDP listening");
			sock_shutdown_tcp(TCPConnectSocket);
			sock_close(TCPConnectSocket);
			if (!nuse_udp) {
				sock_close(UDPConnectSocket);
				delete[] udp_buffer;
				delete[] udp_buffer_receive;
			}
			delete[] tcp_buffer;
			uninitialised = true;
		}
	private:
		struct UDPacketStand {
			UDPacketStand(const char type, const char addr, const char control, char *buffer, const unsigned short bufflen):type(type), addr(addr), control(control), buffer(buffer), bufflen(bufflen) {}
			unsigned short bufflen; char type, addr, control, *buffer;
			~UDPacketStand() {
				delete[] buffer;
			}
		};
		struct UDPacketTimed {
			UDPacketTimed(char addr, char control, char *buffer, unsigned short bufflen):addr(addr), control(control), buffer(buffer), bufflen(bufflen) {}
			unsigned short bufflen; char addr, control, *buffer;
			~UDPacketTimed() {
				delete[] buffer;
			}
		};
		std::vector<UDPacketStand> udp_queue_stand; typename std::vector<UDPacketStand>::iterator udp_start_stand, udp_end_stand;
		std::unordered_map<char, UDPacketTimed> udp_queue_timed; typename std::unordered_map<char, UDPacketTimed>::iterator udp_end_timed;

		void UdpServerThread() {
			std::chrono::steady_clock::time_point tpoint = std::chrono::steady_clock::now();
			std::chrono::steady_clock::duration ctime, duration50 = std::chrono::milliseconds(50);
			typename std::vector<UDPacketStand>::iterator startpstand;
			typename std::unordered_map<char, UDPacketTimed>::iterator startp, endp;
			while (thread_ncancelled) {
				mutex_udp_server_proc.lock();
				for (startp = udp_queue_timed.begin(), endp = udp_queue_timed.end(); startp != endp; ++startp) 
					udp_receiver_callback((DistributionType)(startp->second.control & 0x1), startp->first, startp->second.control >> 4, startp->second.buffer, startp->second.bufflen, startp->second.addr);
				for (startpstand = udp_queue_stand.begin(); startpstand != udp_start_stand; ++startpstand) 
					udp_receiver_callback((DistributionType)(startpstand->control & 0x1), startpstand->type, startpstand->control >> 4, startpstand->buffer, startpstand->bufflen, startpstand->addr);
				
				udp_start_stand = udp_queue_stand.begin();
				udp_queue_timed.clear();
				udp_end_timed = udp_queue_timed.end();
				mutex_udp_server_proc.unlock();
				ctime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tpoint);
				if (ctime < duration50) std::this_thread::sleep_for(duration50 - ctime);
			}
			mutex_udp_server_proc.lock();
			if (thread_released[1]) { udp_queue_timed.clear(); udp_queue_stand.clear(); udp_queue_timed.rehash(0); udp_queue_stand.shrink_to_fit(); udp_listening = false; } else thread_released[0] = true;
			mutex_udp_server_proc.unlock();
		}

		void UdpServerRecUDPThread() {
			unsigned short udp_temp_len; char temp1; unsigned temp2;
			typename std::unordered_map<char, UDPacketTimed>::iterator temp3;
			while (thread_ncancelled) {
				if ((udp_temp_len = sock_recv_udp(UDPConnectSocket, udp_buffer_receive, 1432)) != socket_error) {
					if (*udp_buffer_receive != udp_buffer_receive[8] || udp_buffer_receive[1] != udp_buffer_receive[9]) continue;
					temp1 = udp_buffer_receive[3];
					if (temp1 & 0xC) continue;
					mutex_udp_server_proc.lock();
					if (temp1 & 0x1) {
						if ((temp3 = udp_queue_timed.find(udp_buffer_receive[2])) == udp_end_timed) {
							udp_rec_counter = ((unsigned char)udp_buffer_receive[4] << 24 | (unsigned char)udp_buffer_receive[5] << 16 | (unsigned char)udp_buffer_receive[6] << 8 | (unsigned char)udp_buffer_receive[7]);
							udp_temp_len -= 11;
							udp_queue_timed.emplace(udp_buffer_receive[2], UDPacketTimed(udp_buffer_receive[10], temp1, new char[udp_temp_len], udp_temp_len));
							memcpy((udp_end_timed = --udp_queue_timed.end())->second.buffer, udp_buffer_receive + 11, udp_temp_len);
						} else {
							temp2 = ((unsigned char)udp_buffer_receive[4] << 24 | (unsigned char)udp_buffer_receive[5] << 16 | (unsigned char)udp_buffer_receive[6] << 8 | (unsigned char)udp_buffer_receive[7]);
							if (temp2 >= udp_rec_counter && temp2 - udp_rec_counter < INT_MAX) {
								temp3->second.addr = udp_buffer_receive[10];
								temp3->second.control = temp1;
								udp_rec_counter = temp2;
								if (temp3->second.bufflen < (udp_temp_len -= 11)) {
									delete[] temp3->second.buffer;
									temp3->second.buffer = new char[udp_temp_len];
								}
								temp3->second.bufflen = udp_temp_len;
								memcpy(temp3->second.buffer, udp_buffer_receive + 11, udp_temp_len);
							}
						}
					} else {
						if (udp_start_stand == udp_end_stand) {
							udp_temp_len -= 11;
							udp_queue_stand.emplace_back(udp_buffer_receive[2], udp_buffer_receive[10], temp1, new char[udp_temp_len], udp_temp_len);
							memcpy(((udp_end_stand = udp_start_stand = udp_queue_stand.end()) - 1)->buffer, udp_buffer_receive + 11, udp_temp_len);
						} else {
							udp_start_stand->type = udp_buffer_receive[2];
							udp_start_stand->control = temp1;
							udp_start_stand->addr = udp_buffer_receive[10];
							if (udp_start_stand->bufflen < (udp_temp_len -= 11)) {
								delete[] udp_start_stand->buffer;
								udp_start_stand->buffer = new char[udp_temp_len];
							}
							udp_start_stand->bufflen = udp_temp_len;
							memcpy(udp_start_stand->buffer, udp_buffer_receive + 11, udp_temp_len);
							++udp_start_stand;
						}
					}
					mutex_udp_server_proc.unlock();
				}
			}
			mutex_udp_server_proc.lock();
			if (thread_released[0]) { udp_queue_timed.clear(); udp_queue_stand.clear(); udp_queue_timed.rehash(0); udp_queue_stand.shrink_to_fit(); udp_listening = false; } else thread_released[1] = true;
			mutex_udp_server_proc.unlock();
		}
	};
}