# Game commutation server
###### This server is suitable not only for organizing the exchange of game traffic. It is suitable for any traffic exchange, although it contains some optimizations for game traffic.
Here is the **TCP/UDP server** written on C# and the **client** for this server on C++ (supports TCP and UDP). If you want, you can use it in your project.

A special protocol has been developed for the operation of this server. The protocol provides for the exchange of messages over both TCP and UDP. There is detailed [documentation](https://github.com/mixaisealx/Game-Commutation-Server/blob/master/Game_commutation_server/Server%20Communication%20Protocol%20Specification%20-%20v.1.1.pdf), but only in Russian. If you need to read the documentation, you can use a translator.

The original [word](https://github.com/mixaisealx/Game-Commutation-Server/blob/master/Game_commutation_server/Server%20Communication%20Protocol%20Specification%20-%20v.1.1.docx) documentation file is also provided, which can be edited if necessary to make changes to the documentation.

The repository also contains an example of using a client class. The client class fully implements the functionality provided by the server, so you don't need to write your client class in the case of C++.

What was this server created for? In order to simplify the routing of game traffic "over the local network", as well as to avoid the need to send data from each player to each (there is a "broadcast" in the server to solve this problem). A separate server program also allows to expand the concept of a "local network" without using a VPN, as it allows traffic to pass NAT. Only one "white IP" is enough for the clients to communicate.

<img src="https://user-images.githubusercontent.com/46724356/159118751-a78d3447-253d-41e9-92d1-3cc54516ac01.png" width=448/>

<details>
  <summary>P.S.</summary>
This project was only part of another big project. The big project is dead. There was not a single release. However, this part has been fully completed. Like a big project, this part will no longer be developed.
</details>
