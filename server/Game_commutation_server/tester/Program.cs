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
        static Socket TCPSocket; static byte my_addr;
        static void Main(string[] args)
        {

            Console.WriteLine("---- Game commutation tester ----\n");
            Console.Write("Enter the ip: ");
            IPAddress ip = IPAddress.Parse(Console.ReadLine());
            Console.Write("Enter the number of port: ");
            ushort port = ushort.Parse(Console.ReadLine());

            IPEndPoint ipPoint = new IPEndPoint(ip, port);
            TCPSocket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
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
            string mtext; ushort temp;
            byte[] sarr, narr;
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
                    }
                    else if (mtext.IndexOf("%inds") == 0)
                    {
                        sarr = new byte[] { 1, 0, 3, 1, 0, byte.Parse(mtext.Substring(5, mtext.Length - 5))};
                        TCPSocket.Send(sarr);
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
