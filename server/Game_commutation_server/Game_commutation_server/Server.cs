using System;
using System.Net;
using System.Net.Sockets;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using System.Diagnostics;

namespace GCS //Game Commutation Server
{

    public class Server {
        class UDPacket { public UDPacket(byte type, bool useTime, byte[] data) { this.type = type; this.useTime = useTime; this.data = data; } public byte type; public bool useTime = true; public byte[] data; };
        class UDPpackets { public volatile EndPoint actualIP; public uint sendertime = 0; public volatile List<UDPacket> broadcast = new List<UDPacket>(), unicast = new List<UDPacket>(); };
        class TCPBySendType { public List<byte[]> broadcast = new List<byte[]>(); public volatile List<byte[]> unicast = new List<byte[]>(); };
        class PacketByProtocol { public volatile TCPBySendType tcp = new TCPBySendType(); public volatile UDPpackets udp = new UDPpackets(); };
        class Client { public volatile bool active = true, notsent = false; public volatile PacketByProtocol protocols = new PacketByProtocol(); };

        volatile List<Client> clients = new List<Client>();
     
        EventWaitHandle waitPacketClear = new EventWaitHandle(false, EventResetMode.ManualReset), waitPacketSend = new EventWaitHandle(false, EventResetMode.ManualReset);
        Timer ticker, udp_established; uint time = 0;
        void TimerCallback(object s) {
            waitPacketSend.Reset();
            waitPacketClear.Reset();
            if (time == uint.MaxValue) {
                byte[] resetmsg = new byte[] { 0, 0, 4, 0, 0 };
                for (ushort i = 0; i != clients.Count ; ++i) {
                    clients[i].protocols.tcp.unicast.Add(resetmsg);
                }
                time = 0;
            } else ++time;
        }

        public void Run() {
            Socket listenTCPSocket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            try {
                listenTCPSocket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
                listenTCPSocket.Bind(new IPEndPoint(FNS.StaticMembers.IP_ADDRESS, FNS.StaticMembers.PORT_NUMBER));
                listenTCPSocket.Listen(4);
                Console.WriteLine("[i] TCP server started");
            } catch {
                Console.WriteLine("[!] TCP server start error");
                return;
            }
            UDPSocket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            try {
                UDPSocket.Bind(new IPEndPoint(FNS.StaticMembers.IP_ADDRESS, FNS.StaticMembers.PORT_NUMBER));
                Console.WriteLine("[i] UDP server started");
            } catch {
                Console.WriteLine("[!] UDP server start error");
                return;
            }

            Thread UDPreceiveThread = new Thread(UDPreceiveInterface);
            TaskFactory tfactory = new TaskFactory(TaskCreationOptions.LongRunning, TaskContinuationOptions.LongRunning);

            ticker = new Timer(TimerCallback, null, 0, 50);
            udp_established = new Timer(UDP_established, null, 30000, 30000);
            UDPreceiveThread.Start();
            int eindex;
            while (true) //Wait for connections
            {
                Socket handler = listenTCPSocket.Accept();
                if ((eindex = HaveFreePlace()) != -1 || clients.Count <= FNS.StaticMembers.MAX_GAME_CLIENTS) {
                    if (eindex == -1) {
                        clients.Add(new Client());
                        tfactory.StartNew(() => ClientInterface(handler, (byte)(clients.Count - 1)));
                    } else {
                        clients[eindex].active = true;
                        clients[eindex].notsent = false;
                        tfactory.StartNew(() => ClientInterface(handler, (byte)eindex));
                    }
                } else handler.Disconnect(false);
            }
        }

        byte[] UDP_established_actualiser = new byte[] { 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0 };
        void UDP_established(object s) {
            for (ushort i = 0; i != clients.Count; ++i) {
                if (clients[i].active && clients[i].protocols.udp.actualIP != null)
                    UDPSocket.SendTo(UDP_established_actualiser, clients[i].protocols.udp.actualIP);
            }
        }
        short HaveFreePlace() {
            for (short i = 0; i != clients.Count; ++i)
                if (!clients[i].active)
                    return i;
            return -1;
        }
        bool AllsendsComplete() {
            for (ushort i = 0; i != clients.Count; ++i)
                if (clients[i].active && clients[i].notsent)
                    return false;
            return true;
        }
        bool AllReadyToSend() {
            for (short i = 0; i != clients.Count; ++i)
                if (clients[i].active && !clients[i].notsent)
                    return false;
            return true;
        }
        void ClientInterface(Socket client, byte index) {
            Client mecl = clients[index];
            {   //client notify block
                //                      ushort len|com|ushort len|udp addr
                client.Send(new byte[] { 0x1, 0x0, 0x1, 0x1, 0x0, index }); // 0000 0001  (0x1) - (TCP) flag, set address
            }
            byte[] TCPhead_buffer = new byte[5]; byte[] unicast_addr = new byte[1]; ushort readbytes;
            byte[] temp_buffer; byte[] indexstate = new byte[] { 1, 0, 3, 1, 0, 0 };
            ushort msize; Client tclient;

            bool nwaitsend = true;
            Stopwatch stw = new Stopwatch();
            uint my_time = time;
            try {
                do {
                    stw.Restart();
                    while (client.Available != 0) {
                        readbytes = (ushort)client.Receive(TCPhead_buffer);
                        if (readbytes == 5) {
                            if (TCPhead_buffer[0] == TCPhead_buffer[3] && TCPhead_buffer[1] == TCPhead_buffer[4]) {
                                switch (TCPhead_buffer[2] & 0xF) {
                                    case 0:
                                        msize = BitConverter.ToUInt16(TCPhead_buffer, 0);
                                        temp_buffer = new byte[msize + 5];
                                        TCPhead_buffer.CopyTo(temp_buffer, 0);
                                        client.Receive(temp_buffer, 5, msize, SocketFlags.None);
                                        mecl.protocols.tcp.broadcast.Add(temp_buffer);
                                        //Console.WriteLine("[" + index + " -> all] data: " + Encoding.ASCII.GetString(temp_buffer, 5, msize));
                                        break;
                                    case 2:
                                        msize = BitConverter.ToUInt16(TCPhead_buffer, 0);
                                        client.Receive(unicast_addr);
                                        temp_buffer = new byte[msize + 5];
                                        TCPhead_buffer.CopyTo(temp_buffer, 0);
                                        client.Receive(temp_buffer, 6, msize - 1, SocketFlags.None);
                                        temp_buffer[5] = index;
                                        clients[unicast_addr[0]].protocols.tcp.unicast.Add(temp_buffer);
                                        //Console.WriteLine("[" + index + " -> " + unicast_addr[0] + "] data: " + Encoding.ASCII.GetString(temp_buffer, 6, msize - 1));
                                        break;
                                    case 3:
                                        client.Receive(unicast_addr);
                                        indexstate[5] = clients[unicast_addr[0]].active ? (byte)1 : (byte)0;
                                        mecl.protocols.tcp.unicast.Add(indexstate);
                                        //Console.WriteLine("[" + index + " -> " + index + "] indexState: " + indexstate[5]);
                                        break;
                                    case 1:
                                        byte[] indexes = new byte[clients.Count(n => n.active == true) + 4]; ushort cpos = 5;
                                        BitConverter.GetBytes((ushort)(indexes.Length - 5)).CopyTo(indexes, 0);
                                        indexes[3] = indexes[0]; indexes[4] = indexes[1];
                                        indexes[2] = 1;
                                        for (ushort i = 0; i != clients.Count; ++i)
                                            if (i != index && clients[i].active)
                                                indexes[cpos++] = (byte)i;
                                        mecl.protocols.tcp.unicast.Add(indexes);
                                        //Console.WriteLine("[" + index + " -> " + index + "] indexes: " + string.Join("", indexes.Select(n => n.ToString() + ",").ToArray<string>(), 5, indexes.Length - 5));
                                        break;
                                    case 4:
                                        mecl.protocols.udp.sendertime = 0;
                                        //Console.WriteLine("[" + index + " -> con] action: reset sender counter");
                                        break;
                                }
                            } else throw new SocketException(10054);
                        } else throw new SocketException(10054);
                    }
                    if (time != my_time) {
                        mecl.notsent = true;
                        ClientInterfaceThSend();
                        waitPacketSend.WaitOne();
                        nwaitsend = false;
                        for (ushort i = 0; i != mecl.protocols.tcp.unicast.Count; ++i) {
                            client.Send(mecl.protocols.tcp.unicast[i]);
                        }
                        if (mecl.protocols.udp.actualIP != null) {
                            for (ushort i = 0; i != clients.Count; ++i) {
                                if (i != index && clients[i].active) {
                                    tclient = clients[i];
                                    for (ushort i1 = 0; i1 != tclient.protocols.tcp.broadcast.Count; ++i1) {
                                        client.Send(tclient.protocols.tcp.broadcast[i1]);
                                    }
                                    for (ushort i1 = 0; i1 != tclient.protocols.udp.broadcast.Count; ++i1) {
                                        UDPSocket.SendTo(tclient.protocols.udp.broadcast[i1].data, mecl.protocols.udp.actualIP);
                                    }
                                }
                            }
                            for (ushort i = 0; i != mecl.protocols.udp.unicast.Count; ++i) {
                                UDPSocket.SendTo(mecl.protocols.udp.unicast[i].data, mecl.protocols.udp.actualIP);
                            }
                        } else {
                            for (ushort i = 0; i != clients.Count; ++i) {
                                if (i != index && clients[i].active) {
                                    tclient = clients[i];
                                    for (ushort i1 = 0; i1 != tclient.protocols.tcp.broadcast.Count; ++i1) {
                                        client.Send(tclient.protocols.tcp.broadcast[i1]);
                                    }
                                }
                            }
                        }
                        nwaitsend = true;
                        my_time = time;
                        mecl.notsent = false;
                        ClientInterfaceClear();
                        waitPacketClear.WaitOne();
                    }
                    stw.Stop();
                    if (stw.ElapsedMilliseconds < 20) Thread.Sleep((int)(20 - stw.ElapsedMilliseconds));
                } while (client.Connected);
                throw new SocketException(10054);
            } catch {
                stw.Stop();
                while (true)
                    if (time != my_time) {
                        if (nwaitsend) {
                            mecl.notsent = true;
                            ClientInterfaceThSend();
                            waitPacketSend.WaitOne();
                        }
                        mecl.notsent = false;
                        ClientInterfaceClear();
                        waitPacketClear.WaitOne();
                        mecl.protocols.udp.actualIP = null;
                        mecl.active = false;
                        client.Shutdown(SocketShutdown.Both);
                        client.Close();
                        break;
                    } else Thread.Sleep(20);
            }
        }

        object clientInterfaceThClearLock = new object(), clientInterfaceThSendLock = new object();
        void ClientInterfaceThSend() {
            lock (clientInterfaceThSendLock)
                if (AllReadyToSend())
                    waitPacketSend.Set();

        }
        void ClientInterfaceClear() {
            lock (clientInterfaceThClearLock)
                if (AllsendsComplete()) {
                    for (ushort i = 0; i != clients.Count; ++i)
                        if (clients[i].active) {
                            clients[i].protocols.tcp.broadcast.Clear();
                            clients[i].protocols.tcp.unicast.Clear();
                            clients[i].protocols.udp.broadcast.Clear();
                            clients[i].protocols.udp.unicast.Clear();
                        }
                    waitPacketClear.Set();
                }
        }
        struct UDPReceived { public UDPReceived(EndPoint remoteIP, IEnumerable<byte> data) { this.remoteIP = remoteIP; this.data = data; } public EndPoint remoteIP; public IEnumerable<byte> data;  }
        volatile Queue<UDPReceived> UDPreceiveQueue = new Queue<UDPReceived>();
        volatile Socket UDPSocket;
        void UDPreceiveInterface() {
            TaskFactory tfactory = new TaskFactory(TaskCreationOptions.LongRunning, TaskContinuationOptions.LongRunning);
            tfactory.StartNew(() => ReceivedUDPprocessing());

            EndPoint remoteIP = new IPEndPoint(IPAddress.Any, 0);
            Stopwatch stw = new Stopwatch();
            byte[] buffer = new byte[1432]; ushort readbytes;
            uint my_time = time; byte more40 = 0;
        retry:
            try
            {
                while (true)
                {
                    stw.Restart();
                    if (my_time == time)
                    {
                        if (UDPreceiveQueue.Count > 40)
                        {
                            ++more40;
                            if (more40 == 40)
                            {
                                Console.WriteLine("[i] Add UDP processing worker");
                                tfactory.StartNew(() => ReceivedUDPprocessing());
                            }
                        }
                        else more40 = 0;
                    }
                    else my_time = time;
                    while (UDPSocket.Available != 0)
                    {
                        readbytes = (ushort)UDPSocket.ReceiveFrom(buffer, 1432, SocketFlags.None, ref remoteIP);
                        if (readbytes >= 11)
                        {
                            if (buffer[0] == buffer[8] && buffer[1] == buffer[9] && BitConverter.ToUInt16(buffer, 0) == readbytes - 11)
                            {
                                UDPreceiveQueue.Enqueue(new UDPReceived(remoteIP, buffer.Take(readbytes)));
                            }
                        }
                    }
                    stw.Stop();
                    if (stw.ElapsedMilliseconds < 15) Thread.Sleep((int)(15 - stw.ElapsedMilliseconds));
                }
            }
            catch { goto retry; }
        }

        void ReceivedUDPprocessing() {
            uint my_time = time;
            byte addr, con, type;

            int temp; Client tclient; byte[] databytes; 

            uint timestamp; byte[] timestampb = new byte[4];

            Stopwatch stw = new Stopwatch();
            while (true) {
                stw.Restart();
                while (UDPreceiveQueue.Count != 0) {
                    waitPacketClear.WaitOne();
                    UDPReceived udprcv = UDPreceiveQueue.Dequeue();
                    databytes = udprcv.data.ToArray();
                    addr = databytes[10];
                    if (addr < clients.Count && clients[addr].active) {
                        con = databytes[3];
                        if ((con & 0xC) == 0) { //non-special mode
                            if (databytes.Length - 11 != 0) {
                                if ((con & 0x2) == 0) { //broadcast
                                    if ((con & 0x1) == 1) { //use timestamps
                                        timestamp = BitConverter.ToUInt32(databytes, 4);
                                        type = databytes[2];
                                        temp = -1;
                                        tclient = clients[addr];
                                        for (int i = 0; i != tclient.protocols.udp.broadcast.Count; ++i) 
                                            if (tclient.protocols.udp.broadcast[i].useTime && tclient.protocols.udp.broadcast[i].type == type) { temp = i; break; }
                                        if (temp == -1) {
                                            timestampb = BitConverter.GetBytes(time);
                                            timestampb.CopyTo(databytes, 4);
                                            if (tclient.protocols.udp.actualIP != udprcv.remoteIP)
                                                tclient.protocols.udp.actualIP = udprcv.remoteIP;
                                            tclient.protocols.udp.broadcast.Add(new UDPacket(type, true, databytes));
                                        } else {
                                            if (timestamp >= tclient.protocols.udp.sendertime) {
                                                if (timestamp - tclient.protocols.udp.sendertime < int.MaxValue) {
                                                    tclient.protocols.udp.sendertime = timestamp;
                                                    timestampb = BitConverter.GetBytes(time);
                                                    timestampb.CopyTo(databytes, 4);
                                                    if (tclient.protocols.udp.actualIP != udprcv.remoteIP)
                                                        tclient.protocols.udp.actualIP = udprcv.remoteIP;
                                                    tclient.protocols.udp.broadcast[temp].data = databytes;
                                                }
                                            }
                                        }
                                    } else { //no use timestamps
                                        if (clients[addr].protocols.udp.actualIP != udprcv.remoteIP)
                                            clients[addr].protocols.udp.actualIP = udprcv.remoteIP;
                                        clients[addr].protocols.udp.broadcast.Add(new UDPacket(databytes[2], false, databytes));
                                    }
                                } else { //unicast
                                    if ((con & 0x1) == 1) { //use timestamps
                                        timestamp = BitConverter.ToUInt32(databytes, 4);
                                        type = databytes[2];
                                        temp = -1;
                                        tclient = clients[addr];
                                        for (int i = 0; i != tclient.protocols.udp.unicast.Count; ++i)
                                            if (tclient.protocols.udp.unicast[i].useTime && tclient.protocols.udp.unicast[i].type == type) { temp = i; break; }
                                        if (temp == -1) {
                                            timestampb = BitConverter.GetBytes(time);
                                            timestampb.CopyTo(databytes, 4);
                                            if (tclient.protocols.udp.actualIP != udprcv.remoteIP)
                                                tclient.protocols.udp.actualIP = udprcv.remoteIP;
                                            tclient.protocols.udp.unicast.Add(new UDPacket(type, true, databytes));
                                        } else {
                                            if (timestamp >= tclient.protocols.udp.sendertime) {
                                                if (timestamp - tclient.protocols.udp.sendertime < int.MaxValue) {
                                                    tclient.protocols.udp.sendertime = timestamp;
                                                    timestampb = BitConverter.GetBytes(time);
                                                    timestampb.CopyTo(databytes, 4);
                                                    if (tclient.protocols.udp.actualIP != udprcv.remoteIP)
                                                        tclient.protocols.udp.actualIP = udprcv.remoteIP;
                                                    tclient.protocols.udp.unicast[temp].data = databytes;
                                                }
                                            }
                                        }
                                    } else { //no use timestamps
                                        if (clients[addr].protocols.udp.actualIP != udprcv.remoteIP)
                                            clients[addr].protocols.udp.actualIP = udprcv.remoteIP;
                                        clients[addr].protocols.udp.unicast.Add(new UDPacket(databytes[2], false, databytes));
                                    }
                                }
                            }
                        } else if ((con & 0xC) == 4) {
                            if (clients[addr].protocols.udp.actualIP != udprcv.remoteIP)
                                clients[addr].protocols.udp.actualIP = udprcv.remoteIP;
                        }
                    }
                }
                stw.Stop();
                if (stw.ElapsedMilliseconds < 20) Thread.Sleep((int)(20 - stw.ElapsedMilliseconds));
            }
        }
    }
}
