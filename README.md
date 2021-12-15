# Chat_App
A Simple Chat Application 
---
This program is from my graduate Concurrent, Parallel, and Distributed Programming course. The 
purpose of the project was to create both a client messenger application, and a server which 
would allow multiple clients to communicate with each other. This project makes use of socket 
programming and threading techniques.
---
To build the project:

	Run the command 'make' and the project should compile and produce the 
	executables chat_client.x and chat_server.x. To run  the files, user 
	the command 'file_name config_file', where file_name is either chat_client.x 
	or chat_server.x, and config_file is the corresponding configuration file.
---
Server Design:

	For my server design I used a threaded server, and I used the select function 
	to manage the connected sockets. After accepting a socket, the client would 
	be placed into a set of sock file descriptors. Also, each new user was placed 
	into a vector of structs which would contain all the users' information. 
	Whenever one of the sockets would write to the server, then the server would 
	listen and determine what to do next. I used a pthread to handle the messages 
	from the user. After the user wanted to chat, the pthread would process the 
	information and write to the proper socket(s). I did not use a thread for 
	logging out, instead I just remove the proper socket from the userVector.

Client Design:

	For my client design, I used a single thread that constantly listens to the 
	server, and whenever it can read from the server, it will print to the screen. 
	When the user wants to log out, it sends a simple message to the server in order 
	to have their socket removed, and the server will send back a message letting the 
	client know that they can disconnect from the server.

References:

	For references I used class notes/examples, as well as some tutorial videos I found on
	youtube from a channel called Jacob Sorber. Sorber had two videos that helped 
	me better understand select and pthreads.
