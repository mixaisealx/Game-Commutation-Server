using System;
using System.Net;

namespace Game_commutation_server
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("---- Game commutation server ----\n");
            //IP and Port enter block. If you want to delete this, uncomment default FNS.StaticMembers.IP_ADDRESS and FNS.StaticMembers.PORT_NUMBER in StaticMembers.cs/FNS/StaticMembers
            {
                string input;
                reenter1:
                Console.Write("Enter the ip to listen [" + IPAddress.Any.ToString() +"]: ");
                input = Console.ReadLine();
                if (!IPAddress.TryParse(input, out FNS.StaticMembers.IP_ADDRESS))
                {
                    FNS.StaticMembers.IP_ADDRESS = IPAddress.Any;
                }
                Console.Write("Are you sure (" + FNS.StaticMembers.IP_ADDRESS + ")? (n = No; y = any else) ");
                if (Console.ReadLine() == "n") goto reenter1;
                reenter2:
                Console.Write("Enter the number of port [2024]: ");
                input = Console.ReadLine();
                if (!ushort.TryParse(input, out FNS.StaticMembers.PORT_NUMBER))
                {
                    FNS.StaticMembers.PORT_NUMBER = 2024;
                }
                Console.Write("Are you sure (" + FNS.StaticMembers.PORT_NUMBER + ")? (n = No; y = any else) ");
                if (Console.ReadLine() == "n") goto reenter2;
                reenter3:
                Console.Write("Enter the number of clients [4]: ");
                input = Console.ReadLine();
                if (!byte.TryParse(input, out FNS.StaticMembers.MAX_GAME_CLIENTS))
                {
                    FNS.StaticMembers.MAX_GAME_CLIENTS = 4;
                }
                Console.Write("Are you sure (" + FNS.StaticMembers.MAX_GAME_CLIENTS + ")? (n = No; y = any else) ");
                if (Console.ReadLine() == "n") goto reenter3;
            }

            GCS.Server server = new GCS.Server();
            server.Run(); //Running the server

            Console.WriteLine("---- Game commutation server finished");
            Console.ReadKey();
        }
    }
}
