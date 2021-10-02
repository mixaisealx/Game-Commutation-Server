using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Net;
using System.Net.Sockets;


namespace tester
{
    class Program
    {
        static Socket TCPSocket, UDPSocket; static byte my_addr;
        static void Main(string[] args)
        {

            Console.WriteLine("---- Game commutation tester ----\n");
            Console.Write("Enter the ip: ");
            IPAddress ip = IPAddress.Parse(Console.ReadLine());
            Console.Write("Enter the number of port: ");
            ushort port = ushort.Parse(Console.ReadLine());

            IPEndPoint ipPoint = new IPEndPoint(ip, port);
            TCPSocket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
            UDPSocket = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            TCPSocket.Connect(ipPoint);
            while (TCPSocket.Available == 0) Thread.Sleep(50);
            {
                byte[] addr = new byte[6];
                TCPSocket.Receive(addr);
                my_addr = addr[5];
            }
            Console.WriteLine("[i] Address: " + my_addr);
            Thread th = new Thread(TCPReciever);
            th.Start();
            Thread th1 = new Thread(UDPreceiveInterface);
            th1.Start();
            string mtext; ushort temp; byte tempb;
            byte[] sarr, narr;
            uint packetNumber = 0;
            TCPSocket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
            while (TCPSocket.Connected)
            {
                mtext = Console.ReadLine();
                if (mtext.IndexOf('%')  == 0) {
                    if (mtext == "%gind")
                    {
                        byte[] bytes = new byte[] { 0,0,1,0,0};
                        TCPSocket.Send(bytes);
                    } else if (mtext.IndexOf("%ucst") == 0) {
                        temp = (ushort)mtext.IndexOf(" ");
                        sarr = new byte[mtext.Length - temp + 5];
                        narr = BitConverter.GetBytes((ushort)(mtext.Length - temp));
                        narr.CopyTo(sarr, 0); narr.CopyTo(sarr, 3);
                        sarr[2] = 2;
                        sarr[5] = byte.Parse(mtext.Substring(5, temp - 5));
                        Encoding.ASCII.GetBytes(mtext.Substring(temp + 1)).CopyTo(sarr, 6);
                        TCPSocket.Send(sarr);
                    } else if (mtext.IndexOf("%uup") == 0) {
                        tempb = byte.Parse(mtext.Substring(4, 1));
                        sarr = new byte[mtext.Length + 5];
                        narr = BitConverter.GetBytes((ushort)(mtext.Length - 6));
                        narr.CopyTo(sarr, 0); narr.CopyTo(sarr, 8);
                        sarr[2] = 0; sarr[3] = 2; sarr[10] = tempb;
                        BitConverter.GetBytes(packetNumber++).CopyTo(sarr, 4);
                        Encoding.ASCII.GetBytes(mtext.Substring(6)).CopyTo(sarr, 11);
                        UDPSocket.SendTo(sarr, ipPoint);
                    } else if (mtext.IndexOf("%inds") == 0)
                    {
                        sarr = new byte[] { 1, 0, 3, 1, 0, byte.Parse(mtext.Substring(5, mtext.Length - 5))};
                        TCPSocket.Send(sarr);
                    } 
                    else if (mtext.IndexOf("%udp") == 0) {
                        tempb = byte.Parse(mtext.Substring(4,1));
                        sarr = new byte[mtext.Length + 5];
                        narr = BitConverter.GetBytes((ushort)(mtext.Length - 6));
                        narr.CopyTo(sarr, 0); narr.CopyTo(sarr, 8);
                        sarr[2] = 0; sarr[3] = tempb; sarr[10] = my_addr;
                        BitConverter.GetBytes(packetNumber++).CopyTo(sarr, 4);
                        Encoding.ASCII.GetBytes(mtext.Substring(6)).CopyTo(sarr, 11);
                        UDPSocket.SendTo(sarr, ipPoint);
                        //UDPSocket.SendTo(sarr, ipPoint);
                        //UDPSocket.SendTo(sarr, ipPoint);
                    }
                } else {
                    sarr = new byte[mtext.Length + 5];
                    narr = BitConverter.GetBytes((ushort)mtext.Length);
                    narr.CopyTo(sarr, 0); narr.CopyTo(sarr, 3);
                    sarr[2] = 0;
                    Encoding.ASCII.GetBytes(mtext).CopyTo(sarr, 5);
                    TCPSocket.Send(sarr);
                }
            }
        }
        static void UDPreceiveInterface() {
            byte[] buffer = new byte[1432];

            EndPoint remoteIP = new IPEndPoint(IPAddress.Any, 0);
           
            ushort readbytes;
            while (true) {
                while (UDPSocket.Available != 0) {
                    readbytes = (ushort)UDPSocket.ReceiveFrom(buffer, ref remoteIP);
                    if (readbytes >= 11) {
                        if (buffer[0] == buffer[8] && buffer[1] == buffer[9]) {
                            Console.Write("[UDP][len:" + (readbytes - 11) + "; con:" + int.Parse(Convert.ToString(buffer[3] & 0xF, 2)).ToString("0000") + "; time:" + BitConverter.ToUInt32(buffer, 4) + "; addr:" + buffer[10] + "] data: ");
                            if (readbytes > 11) {
                                Console.WriteLine(Encoding.ASCII.GetString(buffer, 11, readbytes - 11));
                            } else {
                                Console.WriteLine("nodata");
                            }
                        } else {
                            Console.WriteLine("[UDP][Head len:" + readbytes + " - error]");
                        }
                    } else {
                        Console.WriteLine("[UDP][Head len:" + readbytes + " - error]");
                    }
                    
                }
                Thread.Sleep(500);
            }
        }
        static void TCPReciever()
        {
            byte[] TCPhead_buffer = new byte[5]; byte[] byte1 = new byte[1];
            byte[] buff; ushort readbytes; ushort msize;
            while (TCPSocket.Connected)
            {
                if (TCPSocket.Available != 0)
                {
                    readbytes = (ushort)TCPSocket.Receive(TCPhead_buffer);
                    if (readbytes == 5)
                    {
                        if (TCPhead_buffer[0] == TCPhead_buffer[3] && TCPhead_buffer[1] == TCPhead_buffer[4])
                        {
                            msize = BitConverter.ToUInt16(TCPhead_buffer, 0);
                            Console.Write("[len:" + msize + "; con:" + int.Parse(Convert.ToString(TCPhead_buffer[2] & 0xF, 2)).ToString("0000") + "; type:" + int.Parse(Convert.ToString(TCPhead_buffer[2] & 0xF0, 2)).ToString("0000") + "]");
                            if ((TCPhead_buffer[2] & 0xF) == 0)
                            {
                                if (msize > 0)
                                {
                                    buff = new byte[msize];
                                    TCPSocket.Receive(buff);
                                    Console.WriteLine(" text: " + Encoding.ASCII.GetString(buff));
                                }
                            }
                            else if ((TCPhead_buffer[2] & 0xF) == 1)
                            {
                                buff = new byte[msize];
                                TCPSocket.Receive(buff);
                                Console.WriteLine(" indexes: " + string.Join("", buff.Select(n => n.ToString() + ",")));
                            }
                            else if ((TCPhead_buffer[2] & 0xF) == 2)
                            {
                                TCPSocket.Receive(byte1);
                                buff = new byte[msize - 1];
                                TCPSocket.Receive(buff);
                                Console.WriteLine("sender: " + byte1[0] + " unicast: " + Encoding.ASCII.GetString(buff));
                            }
                            else if ((TCPhead_buffer[2] & 0xF) == 3)
                            {
                                TCPSocket.Receive(byte1);
                                Console.WriteLine(" indexstate: " + byte1[0]);
                            }
                        }
                        else Console.WriteLine("[len not equals - error]");
                    }
                    else Console.WriteLine("[Head len:" + readbytes + " - error]");
                }
                Thread.Sleep(500);
            }
        }
    }
}
