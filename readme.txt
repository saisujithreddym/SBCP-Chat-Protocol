ECEN602 HW1 Programming Assignment
----------------------------------

Team Number: 17
Member 1 # Peng, Xuewei (UIN: 824000328)
Member 2 # Mankala, Sai Sujith Reddy  (UIN: 224002333)
---------------------------------------

Design:
--------------------
For the client.c

Use getaddrinfo() to get the struct addrinfo.
Use socket() to get the File Descriptor.
Use connect() to connect to server.
After connect, prepare the content of the message struct with username, pack it in network byte order and 
Use send() to send the message to server.
Then add both the keyboard and the file descriptor into select() read_fds set.
Use select() to check if there is something to read and
     if there is something to read from keyboard,prepare the content of  the message struct with the chat message 
        and send the message to server; 
     if  there is something to read from file descriptor, receive message from server and show it to screen.

For the server.c
Use getaddrinfo() to get the struct addrinfo.
Use socket() to get the File Descriptor.
Use bind() to associate that socket with a port.
Use listen() to see if there is someone want to connect to server from the File Descriptor. 
Every time we have a new File Desciptor, add the file descriptor into select() read_fds set.
Use select() to check if there is something to read and
   if there is something to read from a new client asking to connect, 
      if there is still space, use accept() to accept the new client and assign a new File Descriptor;
   if there is something to read from a existing client to send some message, use recv() to receive the message and
      unpack the message from network byte order to normal order, then based on the content of the message, 
      if the messge is username, need to check if the username can be used or already used by others, 
         if the user name can be used, then reserve it in the names array, 
         if the username can't be used, send back a message and close connection;
      if the message is chat content, send the message to other clients;
      if the message shows that a client exited, send the information to other clients.

---------------------------------------

Description/Comments:
--------------------
1. This package can provide a simple chat service for client and server based on socket programming.
   This package contains three files: server.c, client.c and makefile,
   to generate object files, use: "make -f makefile" in the path of these files in a linux environment.
2. To use the service, first start server by ./server SERVER_IP SERVER_PORT MAX_CLIENTS
   SERVER_IP is the IP address of server, SERVER_PORT is the port of server,
   MAX_CLIENTS is the max number of clients that can connected to the server.
   Then clients can enter the chat room by connecting to server using ./client USERNAME SERVER_IP SERVER_PORT.
   USERNAME is the username the client choose, SERVER_IP is the IP address of server, 
   SERVER_PORT is the port of server.
3. After server run, number of MAX_CLIENTS clients can connect to the server and enter the chat room to chat.
   If more than the number of MAX_CLIENTS clients want to use the server, it has to wait for some one to exit.
   If the client provide a username which already been used by a former client who is still using the service,
   server will send a message "choose another username, force out!" and forced out the client.
   Any client successfully enter the chat room can receive a message of the existed clients in the chat room
   as "names of clients in the chat room: username1 username2 ..."
   After clients successfully enter the chat room, they can talk to each other. When talk, a client just need
   to type the message using keyboard and the message can be sent out when type enter. Other clients in the chat
   room will receive the username and message as "<username>:message"
   Client can exit at any time during the chat and other clients will receive message as "client username* has exited"



Unix command for starting server:
------------------------------------------
./server SERVER_IP SERVER_PORT MAX_CLIENTS

Unix command for starting client:
------------------------------------------
./client USERNAME SERVER_IP SERVER_PORT