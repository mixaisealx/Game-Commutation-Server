#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <exception>
#include <mutex>
#include <chrono>
#include <thread>
#include <vector>
#include <unordered_map>
#include <functional>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

#include "connection_client.h"
using namespace CClient;

using namespace std;


bool ClientProtocolProcessor::Initialise(const char* ipAddress, unsigned short port, bool useUDP) {
	if (!uninitialised) throw ClientProtocolProcessorException("Already initialized");
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ipAddress, &addr.sin_addr.S_un.S_addr) != 1) { throw ClientProtocolProcessorException("Error on transforming 'const char*' to 'IP'"); }
	TCPConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (TCPConnectSocket == INVALID_SOCKET) { throw ClientProtocolProcessorException("Can not create TCP socket"); }
	if (connect(TCPConnectSocket, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) { closesocket(TCPConnectSocket); return false; }
	int temp = 1; char addrbuf[6];
	temp = recv(TCPConnectSocket, addrbuf, 6, 0);
	if (temp != 6) {
		shutdown(TCPConnectSocket, SD_BOTH);
		closesocket(TCPConnectSocket);
		return false;
	}
	address = addrbuf[5];
	nuse_udp = !useUDP;
	if (useUDP) {
		UDPConnectSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (UDPConnectSocket == INVALID_SOCKET) { shutdown(TCPConnectSocket, SD_BOTH); closesocket(TCPConnectSocket); throw ClientProtocolProcessorException("Can not create UDP socket"); }
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
	uninitialised = false;
	return true;
}
void ClientProtocolProcessor::SendTCP(DistributionType distr, const char* buffer, unsigned short buffersize, char userdata4bits, char unicast_address) {
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
	mutex_tcp_sender.unlock();
}

void ClientProtocolProcessor::SendUDP(DistributionType distr, const char* buffer, unsigned short buffersize, char udp_mainTypeUserdata, char userdata4bits, bool packet_GroupingCounting, char unicast_address) {
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
	sendto(UDPConnectSocket, udp_buffer, buffersize + 11, 0, &UDPsendaddress, sizeof(UDPsendaddress));
	mutex_udp_sender.unlock();
}

bool ClientProtocolProcessor::ReceiveTCP(DistributionType &distr, char &unicast_address, char &userdatd4bits, char *&buffer, unsigned short &current_bufflen)
{
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	mutex_tcp_receiver.lock();
systemexec:
	if (recv(TCPConnectSocket, header_tcp_buffer, 5, 0) == 5) {
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
				recv(TCPConnectSocket, buffer, temp, 0);
				break;
			case 2:
				if ((temp = (*header_tcp_buffer | header_tcp_buffer[1] << 8) - 1) > current_bufflen) {
					delete[] buffer;
					current_bufflen = temp;
					buffer = new char[temp];
				} else current_bufflen = temp;
				distr = DistributionType::Unicast;
				userdatd4bits = (header_tcp_buffer[2] & 0xF0) >> 4;
				recv(TCPConnectSocket, &tempaddrbuff, 1, 0);
				unicast_address = tempaddrbuff;
				recv(TCPConnectSocket, buffer, temp, 0);
				break;
			case 3:
				distr = DistributionType::ReceiveSpecilalTCP_State;
				if (current_bufflen == 0) {
					delete[] buffer;
					current_bufflen = 1;
					buffer = new char[1];
				} else current_bufflen = 0;
				recv(TCPConnectSocket, buffer, 1, 0);
				break;
			case 1:
				distr = DistributionType::ReceiveSpecilalTCP_Indexes;
				if ((temp = *header_tcp_buffer | header_tcp_buffer[1] << 8) > current_bufflen) {
					delete[] buffer;
					current_bufflen = temp;
					buffer = new char[temp];
				} else  current_bufflen = temp;
				recv(TCPConnectSocket, buffer, temp, 0);
				break;
			case 4:
				udp_rec_counter = 0;
				goto systemexec;
				break;
			}
			mutex_tcp_receiver.unlock();
			return true;
		} else {
			Finalise();
			mutex_tcp_receiver.unlock();
			throw ClientProtocolProcessorException("Uncorrect packet");
		}
	} else {
		Finalise();
		mutex_tcp_receiver.unlock();
		return false;
	}
}

void ClientProtocolProcessor::SendTCPSpecial(DistributionType specialReceiveType, char address) {
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	mutex_tcp_special_sender.lock();
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
	mutex_tcp_special_sender.unlock();
}

void ClientProtocolProcessor::Finalise() {
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	shutdown(TCPConnectSocket, SD_BOTH);
	closesocket(TCPConnectSocket);
	if (!nuse_udp) { 
		closesocket(UDPConnectSocket);
		delete[] udp_buffer;
		delete[] udp_buffer_receive;
	}
	delete[] tcp_buffer;
	uninitialised = true;
}

void ClientProtocolProcessor::ReceiveEveryUDP(DistributionType &distr, char &udp_mainTypeUserdata, char &userdatd4bits, char *buffer, unsigned short &current_bufflen, char &unicast_address) {
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
	if (udp_listening) throw ClientProtocolProcessorException("UDP already listening");
	udp_listening = true;
systemexec:
	if ((current_bufflen = recvfrom(UDPConnectSocket, udp_buffer_receive, 1432, 0, nullptr, nullptr)) != SOCKET_ERROR) {
		if (*udp_buffer_receive != udp_buffer_receive[8] || udp_buffer_receive[1] != udp_buffer_receive[9]) goto systemexec;
		if (!(udp_buffer_receive[3] & 0xC)) goto systemexec;
		current_bufflen -= 11;
		userdatd4bits = (header_tcp_buffer[3] & 0xF0) >> 4;
		udp_mainTypeUserdata = header_tcp_buffer[2];
		if (udp_buffer_receive[3] & 0x2) {
			distr = DistributionType::Unicast;
			unicast_address = header_tcp_buffer[10];
		} else 
			distr = DistributionType::Broadcast;
		memcpy(buffer, udp_buffer_receive + 11, current_bufflen);
		udp_listening = false;
	} else goto systemexec;
}

void CClient::ClientProtocolProcessor::UdpServerThread(UDPReceiverCallback callback) {
	chrono::steady_clock::time_point tpoint = chrono::steady_clock::now();
	chrono::steady_clock::duration ctime;
	vector<UDPacketStand>::iterator startpstand;
	unordered_map<char, UDPacketTimed>::iterator startp, endp;
	while (thread_ncancelled) {
		mutex_udp_server_proc.lock();
		for (startp = udp_queue_timed.begin(), endp = udp_queue_timed.end(); startp != endp; ++startp)
			callback((DistributionType)(startp->second.control & 0x1), startp->first, startp->second.control >> 4, startp->second.buffer, startp->second.bufflen, startp->second.addr);
		for (startpstand = udp_queue_stand.begin(); startpstand != udp_start_stand; ++startpstand)
			callback((DistributionType)(startpstand->control & 0x1), startpstand->type, startpstand->control >> 4, startpstand->buffer, startpstand->bufflen, startpstand->addr);
		udp_start_stand = udp_queue_stand.begin();
		udp_queue_timed.clear();
		udp_end_timed = udp_queue_timed.end();
		mutex_udp_server_proc.unlock();
		ctime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tpoint);
		if (ctime < 50ms) this_thread::sleep_for(50ms - ctime);
	}
	thread_release.lock();
	if (thread_released[1]) { udp_queue_timed.clear(); udp_queue_stand.clear(); udp_queue_timed.rehash(0); udp_queue_stand.shrink_to_fit(); udp_listening = false; }
	else thread_released[0] = true;
	thread_release.unlock();
}

void CClient::ClientProtocolProcessor::UdpServerRecUDPThread() {
	unsigned short udp_temp_len; char temp1; unsigned temp2;
	std::unordered_map<char, UDPacketTimed>::iterator temp3;
	while (thread_ncancelled) {
		if ((udp_temp_len = recvfrom(UDPConnectSocket, udp_buffer_receive, 1432, 0, nullptr, nullptr)) != SOCKET_ERROR) {
			if (*udp_buffer_receive != udp_buffer_receive[8] || udp_buffer_receive[1] != udp_buffer_receive[9]) continue;
			temp1 = udp_buffer_receive[3];
			if (temp1 & 0xC) continue;
			mutex_udp_server_proc.lock();
			if (temp1 & 0x1) {
				if ((temp3 = udp_queue_timed.find(udp_buffer_receive[2])) == udp_end_timed) {
					udp_rec_counter = ((unsigned char)udp_buffer_receive[4] << 24 | (unsigned char)udp_buffer_receive[5] << 16 | (unsigned char)udp_buffer_receive[6] << 8 | (unsigned char)udp_buffer_receive[7]);
					udp_queue_timed.emplace(udp_buffer_receive[2], UDPacketTimed(udp_buffer_receive[10], temp1, new char[udp_temp_len -= 11], udp_temp_len));
					memcpy((udp_end_timed = --udp_queue_timed.end())->second.buffer, udp_buffer_receive + 11, udp_temp_len);
				} else {
					temp2 = ((unsigned char)udp_buffer_receive[4] << 24 | (unsigned char)udp_buffer_receive[5] << 16 | (unsigned char)udp_buffer_receive[6] << 8 | (unsigned char)udp_buffer_receive[7]);
					if (temp2 >= udp_rec_counter && temp2 - udp_rec_counter < INT_MAX) {
						temp3->second.addr = udp_buffer_receive[10];
						temp3->second.control = temp1;
						udp_rec_counter = temp2;
						temp3->second.bufflen = udp_temp_len -= 11;
						delete[] temp3->second.buffer;
						temp3->second.buffer = new char[udp_temp_len];
						memcpy(temp3->second.buffer, udp_buffer_receive + 11, udp_temp_len);
					}
				}
			} else {
				if (udp_start_stand == udp_end_stand) {
					udp_queue_stand.emplace_back(header_tcp_buffer[2], header_tcp_buffer[10], temp1, new char[udp_temp_len -= 11], udp_temp_len);
					memcpy(((udp_end_stand = udp_start_stand = udp_queue_stand.end()) - 1)->buffer, udp_buffer_receive + 11, udp_temp_len);
				} else {
					udp_start_stand->type = header_tcp_buffer[2];
					udp_start_stand->control = temp1;
					udp_start_stand->addr = header_tcp_buffer[10];
					udp_start_stand->bufflen = udp_temp_len -= 11;
					udp_start_stand->buffer = new char[udp_temp_len];
					memcpy(udp_start_stand->buffer, udp_buffer_receive + 11, udp_temp_len);
					++udp_start_stand;
				}
			}
			mutex_udp_server_proc.unlock();
		}
	}
	thread_release.lock();
	if (thread_released[0]) { udp_queue_timed.clear(); udp_queue_stand.clear(); udp_queue_timed.rehash(0); udp_queue_stand.shrink_to_fit(); udp_listening = false; }
	else thread_released[1] = true;
	thread_release.unlock();
}

bool CClient::ClientProtocolProcessor::StartUDPReceiveServer(UDPReceiverCallback callback) {
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
	if (udp_listening) return false;
	udp_listening = thread_ncancelled = true;
	thread_released[0] = thread_released[1] = false;
	udp_end_stand = udp_start_stand = udp_queue_stand.end();
	udp_end_timed = udp_queue_timed.end();
	udp_server_thread = thread([&]() { UdpServerThread(callback); });
	udp_server_rec_thread = thread([&]() { UdpServerRecUDPThread(); });
	return true;
}

bool CClient::ClientProtocolProcessor::StopUDPReceiveServer() {
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	if (nuse_udp) throw ClientProtocolProcessorException("Uninitialized UDP");
	if (!thread_ncancelled) return false;
	thread_ncancelled = false;
	return true;
}

char ClientProtocolProcessor::GetMyAddress() {
	if (uninitialised) throw ClientProtocolProcessorException("Uninitialized");
	return address;
}

bool CClient::InitialeseGlobalWSA()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) return false;
	return true;
}

void CClient::CleanupGlobalWSA()
{
	WSACleanup();
}