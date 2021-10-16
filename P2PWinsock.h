//Copyright © 2020 Nicolas Ortiz
//MIT License
//This simple demo allows two computers to send a message to one another continously
//First both will try and send the message at more and less the same time
//Then they'll each wait until they get a response
//Once they get it they'll send the same message again
//If at some point they disconnect they'll try 10 times to reconnect.
//They'll wait 500ms interval before sending messages and/or between reconnection attempts.
//Also they'll assign themselves a random number as ID so you can identify each peer. 
//This ID will be shown when receiving messages ("Message from #") and on the title screen of each peer's console window.

//To use it follow the prompt and enter a port to connect to (computer to connect to) and a port to receive at (on computer is running on)
//Note that you will need to modify IP to choose what computer to connect to below (see line 127)

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib,"ws2_32.lib") //Winsock Library

#include <stdio.h>
#include <string>
#include<winsock2.h>
#include <iostream>

static SOCKET s;

BOOL WINAPI ConsoleHandlerRoutine(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_CLOSE_EVENT)
	{
		if (closesocket(s) == SOCKET_ERROR)
			std::cout << "Could not close socket" << std::endl;

		if(WSACleanup() == SOCKET_ERROR)
			std::cout << "Could not cleanup WSA" << std::endl;
	}

	return FALSE;
}


void Run(u_char bound_port, const char* connect_ip, u_char connect_port)
{
	srand(time(NULL));

	auto identifier = rand();

	SetConsoleTitleA(std::to_string(identifier).c_str());

	WSADATA wsa;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		throw new std::string("Could not initialize server WSA:" + WSAGetLastError());

reconnect:
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
		throw new std::string("Could not create socket : " + std::to_string(WSAGetLastError()));

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(bound_port);

	//Bind
	if (bind(s, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
		printf("Bind failed with error code : %d", WSAGetLastError());

	std::cout << "Bound to " << std::to_string((int)bound_port) << std::endl;

	server.sin_addr.s_addr = inet_addr(connect_ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(connect_port);

	while (connect(s, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		std::cout << "Attempting connection..." << std::endl;
		Sleep(1000);
	}

	std::cout << "Connected to " << std::to_string((int)connect_port) << std::endl;

	int tries = 10;
	while (true)
	{
		int recv_size;
		char server_reply[2000];

		std::cout << "Attempting echo!" << std::endl;
		auto msg = (std::string("Hello from ") + std::to_string(identifier));

		if (send(s, msg.c_str(), strlen(msg.c_str()), 0) >= 0)
		{
			if ((recv_size = recv(s, server_reply, 2000, 0)) != SOCKET_ERROR)
			{
				server_reply[recv_size] = '\0';
				std::cout << "Recv: " << server_reply << std::endl;
			}
		}
		else
			tries--;

		if (tries <= 0)
		{
			closesocket(s);
			std::cout << "Disconnected" << std::endl;
			goto reconnect;
		}

		Sleep(500);
	}

	if (closesocket(s) == SOCKET_ERROR)
		printf("Could not close client socket : %d\n", WSAGetLastError());


	if (WSACleanup() == SOCKET_ERROR)
		throw new std::string("Could not close WSA: " + WSAGetLastError());
}

int main()
{
	std::cout << "Enter connection port and bound port (aa bb): ";
	int cp, bp;
	std::cin >> cp >> bp;
	
	std::cout << std::endl;
	BOOL ret = SetConsoleCtrlHandler(ConsoleHandlerRoutine, TRUE);

	try
	{
    //Change to another IP address to connect to other computers
    //By default set to localhost to run both Peers on same computer for testing
		Run((u_char)bp, "127.0.0.1", (u_char)cp);
	}
	catch (std::string& s)
	{
		std::cout << s << std::endl;
	}
	catch (...)
	{
		std::cout << "Caught unknown exception!" << std::endl;
	}

	std::cin.get();
	return 0;
}
