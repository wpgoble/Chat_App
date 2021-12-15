#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>

using namespace std;
const unsigned MAXBUFLEN = 512;
int sockfd;

#define TRUE 1
#define FALSE 0
void* process_connection(void*);
int contactServer(string, string, string);

struct Users
{
	string name;
	string msg;
};

string userName;

// main function for the client
int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		cout << "Usage: chat_client.x config_file\n";
		exit(1);
	}

	string servHost;
	string servPort;
	// unsigned port = 0;  
	ifstream myfile(argv[1]);
	// read file and get data
	if(myfile.is_open())
	{
		string temp;
		getline(myfile, temp);
		servHost = temp.substr(10);
		servHost.erase(servHost.end() - 1);
		getline(myfile, temp);
		servPort = temp.substr(10);
		servPort.erase(servPort.end() - 1);
		//port = stoi(temp.substr(10));
		myfile.close();
	}
	else
	{
		cout << "Unable to open file.\n";
		return EXIT_FAILURE;
	}	

	cout << "ServHost = " << servHost;
	cout << ", ServPort = " << servPort  << endl; 

	string input;
	//while(getline(cin, input))
	while(TRUE)
	{
		cout << "$ ";
		getline(cin, input);
		if(input == "exit")
		{
			//if(connected)
			//	close(sockfd);
			break;
		}
		else
		{
			size_t location = input.find_first_of(" ");
			string cmd = input.substr(0, location);
			userName = input.substr(location+1);
			bool hasName = FALSE;
			if(userName.find_first_not_of(' ') != std::string::npos)
				hasName = TRUE;

			if(cmd == "login")
			{
				if(hasName)
				{
					if(contactServer(servHost, servPort, input) == -1)
						cout << "Server Error.\n";
				}
				else
					cout << "Please include a user name.\n";
			}
		}
	}
	
	return EXIT_SUCCESS;
}

// here we want to connect to the server and start listening for
// user input
int contactServer(string servHost, string servPort, string username)
{
	int rv;
	int flag;
	struct addrinfo hints;
	struct addrinfo* res;
	struct addrinfo* ressave;
	pthread_t tid;
	const char* host = servHost.c_str();
	const char* port = servPort.c_str();

	// lets start to connect to the server
	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(host, port, &hints, &res)) != 0)
	{
		cout << "Getaddrinfo wrong: " << gai_strerror(rv) << endl;
		return -1;
	}

	ressave = res;
	flag = 0;
	
	do{
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if(sockfd < 0)
			continue;
		if(connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
		{
			flag = 1;
			break;
		}
		close(sockfd);
	}while((res = res->ai_next) != NULL);
	freeaddrinfo(ressave);	

	if(flag == 0)
	{
		fprintf(stderr, "cannot connect.\n");
		return -1;
	}

	// Here we want to constantly listen to the server and see if 
	// anyone wants to write us something
	pthread_create(&tid, NULL, &process_connection, NULL);

	// our initial write to the server where we send our undername
	write(sockfd, username.c_str(), username.length());
	cout << "Connected to server.\n";	// success message
	
	// get new user input
	string input;
	while(getline(cin, input))
	{
		size_t location = input.find_first_of(" ");
		string cmd = input.substr(0, location);

		// when we log out we want the server to know that we 
		// want to disconnect
		if(cmd == "logout")
		{
			string LOGOUT = "logout";
			write(sockfd, LOGOUT.c_str(), LOGOUT.length());
			//cout << "User wants to log out.\n";
			close(sockfd);
			break;
		}
		else if(cmd == "chat")
		{
			// this lets the server know that we want to chat, 
			// the message will be process and handled by the 
			// server
			string CHAT = "chat";
			write(sockfd, CHAT.c_str(), CHAT.length());
			write(sockfd, input.c_str(), input.length());
		}
		else
		{
			// invalid command, we only want the server to chat or logout,
			// if they are logged out, then they can relogin from the main loop
			cout << "Invalid command. Try using chat or logout.\n";
		}
	}

	return 1;		
}

void *process_connection(void *arg) {
	int n;
	char buf[MAXBUFLEN];
	pthread_detach(pthread_self());
	while (TRUE) {
		n = read(sockfd, buf, MAXBUFLEN);

		if (n <= 0) {
			if (n == 0) {
				cout << "server closed" << endl;
			}
			 else {
				cout << "something wrong" << endl;
			}
			close(sockfd);
			// we directly exit the whole process from the error.
			exit(1);
		}

		buf[n] = '\0';
		string output(buf);
		// here we want to just test if the user tried to logout
		string message = userName + " >> logout\0";
		string log = output.substr(output.length()-6);
		
		// block other users from seeing the logout, and we want
		// to terminate the pthread so we stop listening. 
		if(output == message)
		{
			string test = "test";
			write(sockfd, test.c_str(), test.length());
			pthread_exit((void*) 0);
		}
		else
		{
			// if the current user does not want to logout,
			// then we want to print the message
			if(log != "logout\0")
				cout << buf << endl;
		}
		// clear the message 
		memset(buf, 0, MAXBUFLEN);
	}
}

