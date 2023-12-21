#include <iostream>
#include <WinSock2.h>
#include <thread>
#include <ws2tcpip.h>

class Client
{
public:
	Client(std::string& serverIP, int port) : m_socket(INVALID_SOCKET), m_serverAddress(serverIP), m_port(port) {};
	~Client()
	{
		WSACleanup();
		if (m_socket != INVALID_SOCKET)
		{
			closesocket(m_socket);
		}
	}

	bool connectToServer()
	{
		WSADATA wsData;
		if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
			std::cerr << "WSAStartup failed: " << WSAGetLastError() << '\n';
			return false;
		}

		m_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (m_socket == INVALID_SOCKET)
		{
			std::cerr << "socket failed: " << WSAGetLastError() << '\n';
			WSACleanup();
			return false;
		}

		sockaddr_in hint;
		hint.sin_family = AF_INET;
		hint.sin_port = htons(m_port);
		inet_pton(AF_INET, m_serverAddress.c_str(), &hint.sin_addr);

		if (connect(m_socket, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR)
		{
			std::cout << "Connect failed: " << WSAGetLastError() << '\n';
			closesocket(m_socket);
			WSACleanup();
			return false;
		}

		return true;
	}

	bool SendInitCommand()
	{
		std::string command = "INI";
		return send(m_socket, command.c_str(), command.size(), 0) != SOCKET_ERROR;
	}

	bool SendConfigurationData(std::string query)
	{
		int queryLength = htonl(query.length());
		int normalQueryLength = query.length();
		if (send(m_socket, reinterpret_cast<char*>(&queryLength), sizeof(int), 0) == SOCKET_ERROR)
		{
			std::cerr << "Send query failed: " << WSAGetLastError() << '\n';
			return false;
		}

		if (send(m_socket, query.c_str(), sizeof(char) * normalQueryLength, 0) == SOCKET_ERROR)
		{
			std::cerr << "Send query failed: " << WSAGetLastError() << '\n';
			return false;
		}

		return true;
	}

	bool sendStartCommand()
	{
		std::string command = "STA";
		return send(m_socket, command.c_str(), command.size(), 0) != SOCKET_ERROR;
	}

	std::string ReceiveResponse()
	{
		char buffer[1024];
		int result = recv(m_socket, buffer, 1024, 0);
		while (result == SOCKET_ERROR || result == 0)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			result = recv(m_socket, buffer, 1024, 0);
			continue;
			/*std::cerr << "Recv response failed: " << WSAGetLastError() << '\n';
			closesocket(m_socket);
			WSACleanup();
			return "";*/
		}
		buffer[result] = '\0';

		return std::string(buffer);
	}

	std::string ReceiveStartingResponse()
	{
		std::string response = ReceiveResponse();
		if (response != "STARTING") {
			std::cerr << "Unexpected response from server: " << response << '\n';
			closesocket(m_socket);
			WSACleanup();
			return "";
		}
		return response;
	}

	std::string SendGetCommand()
	{
		std::string command = "GET";
		send(m_socket, command.c_str(), command.size(), 0);
		return ReceiveResponse();
	}

	std::string GetResult()
	{
		std::string response = SendGetCommand();
		std::cout << "response from the server: " << response << '\n';

		if (response.substr(0, 6) == "STATE:") {
			response = response.substr(6);
		}
		if (response.substr(0, 9) == "COMPLETED") {
			response = response.substr(9);
			if (response.substr(1, 7) == "RESULT:") {
				return response.substr(8);
			}
		}
		else {
			std::cerr << "Invalid response format" << '\n';
			closesocket(m_socket);
			WSACleanup();
			return "-1";
		}
	}

private:
	SOCKET m_socket;
	std::string m_serverAddress;
	int m_port;
};

//int main()
//{
//	std::string IP = "127.0.0.1";
//	auto client = Client(IP, 1234);
//	std::string query = "meanwhile";
//
//	std::this_thread::sleep_for(std::chrono::seconds(4));
//	auto result = client.connectToServer();
//	std::cout << "client connected" << '\n';
//	result = client.SendInitCommand();
//	std::cout << "INIT command sent" << '\n';
//	if (!result) {
//		std::cerr << "Server initialization failed." << '\n';
//		return 1;
//	}
//	auto response = client.ReceiveResponse();
//	if (response != "ACK") {
//		std::cerr << "ACK has not been sent from the server." << '\n';
//		return 1;
//	}
//	std::cout << "Received ACK for INIT" << '\n';
//
//	//int rowsToSend = htonl(rows), colsToSend = htonl(cols), threadsCountToSend = htonl(threadsCount);
//	client.SendConfigurationData(query);
//	std::cout << "Data configuration has been sent" << '\n';
//	response = client.ReceiveResponse();
//	if (response != "ACK") {
//		std::cerr << "ACK has not been sent from the server." << '\n';
//		return 1;
//	}
//	std::cout << "Received ACK for Config" << '\n';
//	client.sendStartCommand();
//	std::cout << "Start command has been sent" << '\n';
//
//	response = client.ReceiveResponse();
//	if (response != "STARTING") {
//		std::cerr << "Starting has not been sent from the server." << '\n';
//	}
//	std::cout << "Received Starting response" << '\n';
//	std::this_thread::sleep_for(std::chrono::seconds(6));
//	response = client.GetResult();
//
//	std::cout << "The result is: " << response << '\n';
//
//	char key;
//	std::cin >> key;
//	return 0;
//}