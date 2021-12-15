#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <vector>
#include <pthread.h>
#include <signal.h>

using namespace std;

//unsigned port = 5100;
const unsigned MAXBUFLEN = 512;

#define TRUE 1
#define FALSE 0
void runServer(unsigned);
void showAllUsers();
void makeNewUser(struct sockaddr_in, int, string);
void clearList(int);
void removeUser(int);

void* handle_message(void*);
void* handleUser(void*);

void* process_connection(void*);
unsigned userCount = 0;
int serv_sockfd;

typedef struct
{
	string name;
	string ip;
	int port;
	int sockfd;
}User;
vector<User> userVector;

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		cout << "usage: client_server.x config_file\n";
		exit(1);
	}

	signal(SIGINT, clearList);
	unsigned port = 0;
	ifstream myfile(argv[1]);
	if(myfile.is_open())
	{
		string temp;
		// This should only contain a single line, namely port. So we 
		// will only grab it once.
		getline(myfile, temp);
		port = stoi(temp.substr(5));
		myfile.close();	
	}
	else
		cout << "Unable to open file.\n";

	runServer(port);

	return EXIT_SUCCESS;
}

// here we want to run the server and listen for clients
void runServer(unsigned port)
{
	//int serv_sockfd;
	int max_socket;
	//int client_sockfd;
	int* sock_ptr;
	struct sockaddr_in serv_addr;
	struct sockaddr_in client_addr;
	socklen_t sock_len;
	pthread_t tid;

	cout << "port = " << port << endl;
	serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// lets establish the server and start listening
	bzero((void*)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	
	bind(serv_sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	// here the backlog arguement is 5 just so we can have multiple users 
	listen(serv_sockfd, 5);

	// next we want to make some sets to manage which sockets are trying 
	// to send us something.
	vector<User> users;
	fd_set current_sockets, ready_sockets;
	FD_ZERO(&current_sockets);
	FD_SET(serv_sockfd, &current_sockets);
	max_socket = serv_sockfd;
	// now we listen
	while(TRUE)
	{
		// save the copy of current_sockets
		ready_sockets = current_sockets;
		
		// Here we will select the socket that wants to talk with us
		if(select(max_socket + 1, &ready_sockets, NULL, NULL, NULL) < 0)
		{
			fprintf(stderr, "select error.");
			exit(1);
		}

		// next we want to go through the sockets to see if anyone 
		// wants to send us something
		for(int i = 0; i < max_socket+1; i++)
		{
			if(FD_ISSET(i, &ready_sockets))
			{
				// new connection, create and add user to our userVector
				if(i == serv_sockfd)
				{
					sock_len = sizeof(client_addr);
					int client_sock = accept(serv_sockfd, (struct sockaddr*)&client_addr, &sock_len);
					if(client_sock < 0)
						perror("Select error.");
					else
					{
						char buf[MAXBUFLEN];
						ssize_t len = read(client_sock, buf, MAXBUFLEN);
						buf[len] = '\0';
						string input(buf);
						ssize_t endLogin = input.find_first_of(" ");
						string name = input.substr(endLogin+1);
						makeNewUser(client_addr, client_sock, name);
						//listenForNewUser(client_addr, client_sock);
						FD_SET(client_sock, &current_sockets);
						showAllUsers();
					}

					// this will update the max value if we keep getting more sockets
					if(client_sock > max_socket)
						max_socket = client_sock;
				}
				// listen for already connected user, let's see what they want
				else
				{
					// collect the command and determine what we want to do
					char cmd_arr[MAXBUFLEN];
					ssize_t cmd_len = read(i, cmd_arr, MAXBUFLEN);
					cmd_arr[cmd_len] = '\0';
					fflush(stdout);
					string cmd(cmd_arr);

					// if we want to chat, then let's create the pthread that
					// will handle the conversation.
					if(cmd == "chat\0")
					{
						sock_ptr = (int*)malloc(sizeof(int));
						*sock_ptr = i;
						pthread_create(&tid, NULL, &handle_message, (void*)sock_ptr);
					}
					// if the user is trying to logout, then we just need to remove the
					// user from our vector. The user will take care of closing it's side 
					// of the connection, we will close ours in a minute.
					if(cmd == "logout")
					{
						removeUser(i);
					}

					FD_CLR(i, &current_sockets);
				}
			}
		}
	}
	close(serv_sockfd);
}

void* handle_message(void* arg)
{
	int sockfd_local;
	ssize_t end;
	char temp[MAXBUFLEN];
	string user = "";

	sockfd_local = *((int*)arg);
	free(arg);

	pthread_detach(pthread_self());

	for(unsigned i = 0; i < userVector.size(); i++)
		if(userVector[i].sockfd == sockfd_local)
			user = userVector[i].name;

	while((end = read(sockfd_local, temp, MAXBUFLEN)) > 0)
	{
		temp[end] = '\0';

		// the chat command is sent constantly from the user, so
		// we just want to ignore it here, could have probably done 
		// this differently.
		if(!strcmp(temp, "chat\0"))
			continue;
		
		// let's process what the user has sent us, and determine 
		// who needs to receive the message
		string input(temp);
		size_t space = input.find_first_of(' ');
		string cmd = input.substr(0, space);
		string parsedInput = input.substr(space+1);

		// the user has a private message they want to send.
		if(input[space+1] == '@')
		{
			size_t sndSpace = parsedInput.find_first_of(' ');
			string target = parsedInput.substr(1, sndSpace-1);
			string output = user + " >> " + parsedInput.substr(sndSpace+1);
			int targetFound = 0;

			// here we will check through the userVector and see if the 
			// target is currently logged in, if not, then we will send 
			// a message that the user is not logged in.
			for(User u : userVector)
			{
				if(u.name == target)
				{
					targetFound = 1;
					write(u.sockfd, output.c_str(), output.length());
				}
			}

			if(targetFound == 0)
			{
				output = target + " is not currently logged in, try again later.";
				write(sockfd_local, output.c_str(), output.length());
			}
		}
		// The user wants to send a public message, let's send this to everyone, 
		// including the sender.
		else
		{
			string output = user + " >> " + parsedInput;
			for(User u : userVector)
			{
				write(u.sockfd, output.c_str(), output.length());
			}
		}
	}

	// Here we just have some error handling
	if(end == 0)
	{
		cout << user + " has closed their connection.\n";
		removeUser(sockfd_local);	// the user disconnected, so let's just remove them from our list.
	}
	else
		cout << "Something went wrong." << endl;

	close(sockfd_local);
	return NULL;
}

// print out all the current users
void showAllUsers()
{
	cout << "---------------- Online Clients ----------------\n";
	for(unsigned itr = 0; itr < userCount; itr++)
		cout << "User name: " << userVector[itr].name << endl;
	cout << "------------------------------------------------\n";
}

// Fills out the user data and pushes it into our vector.
void makeNewUser(struct sockaddr_in client, int sock, string name)
{
	User foo;
	foo.name = name;
	foo.ip = inet_ntoa(client.sin_addr);
	foo.port = ntohs(client.sin_port);
	foo.sockfd = sock;
	userVector.push_back(foo);
	userCount++;
}

// The server has been terminated, let's clean this up first 
// by removing all the online users. 
void clearList(int sig)
{
	signal(sig, SIG_IGN);
	printf("\n");
	for(unsigned i = 0; i < userVector.size(); i++)
	{
		cout << "Closing user: " << userVector[i].name << endl;
		close(userVector[i].sockfd);
	}
	close(serv_sockfd);
	cout << "Terminating server. Good Bye." << endl;
	exit(0);
}

void removeUser(int sock)
{
	// Here we want to search through the userVector and remove
	// our target socket
	for(unsigned i = 0; i < userVector.size(); i++)
	{
		if(userVector[i].sockfd == sock)
		{
			userVector.erase(userVector.begin() + i);
			userCount--;
			break;
		}
	}
	// Reprint the list of people currently logged into 
	// the program
	showAllUsers();
}	
