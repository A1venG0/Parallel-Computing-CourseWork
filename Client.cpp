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

	bool SendConfigurationData(int rowsToSend, int colsToSend, int threadsCount, int rows, int cols, int* data)
	{
		if (send(m_socket, (char*)&rowsToSend, sizeof(int), 0) == SOCKET_ERROR ||
			send(m_socket, (char*)&colsToSend, sizeof(int), 0) == SOCKET_ERROR)
		{
			std::cerr << "Send matrix size failed: " << WSAGetLastError() << '\n';
			return false;
		}

		if (send(m_socket, (char*)&threadsCount, sizeof(int), 0) == SOCKET_ERROR)
		{
			std::cerr << "Send threads count failed: " << WSAGetLastError() << '\n';
			return false;
		}

		if (send(m_socket, reinterpret_cast<char*>(data), sizeof(int) * rows * cols, 0) == SOCKET_ERROR)
		{
			std::cerr << "Send matrix failed: " << WSAGetLastError() << '\n';
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
//	int rows = 3;
//	int cols = 3;
//	const int threadsCount = 4;
//	//std::unique_ptr<int[]> data = std::make_unique<int[]>(rows * cols);
//	int* data = new int[rows * cols];
//	for (int i = 0; i < rows; i++)
//	{
//		for (int j = 0; j < cols; j++)
//		{
//			data[i * cols + j] = i + j;
//		}
//	}
//
//	for (int i = 0; i < rows; i++)
//	{
//		for (int j = 0; j < cols; j++)
//			std::cout << data[i * cols + j] << ' ';
//	}
//
//	std::cout << '\n';
//
//	std::this_thread::sleep_for(std::chrono::seconds(4));
//	auto result = client.connectToServer();
//	std::cout << "client connected" << '\n';
//	result = client.SendInitCommand();
//	std::cout << "INIT command sent" << '\n';
//	if (!result) {
//		std::cerr << "Server initialization failed." << '\n';
//		delete[] data;
//		return 1;
//	}
//	auto response = client.ReceiveResponse();
//	if (response != "ACK") {
//		std::cerr << "ACK has not been sent from the server." << '\n';
//		delete[] data;
//		return 1;
//	}
//	std::cout << "Received ACK for INIT" << '\n';
//	/*for (int i = 0; i < rows; i++)
//	{
//		for (int j = 0; j < cols; j++)
//		{
//			data[i * cols + j] = htonl(data[i * cols + j]);
//		}
//	}*/
//	int rowsToSend = htonl(rows), colsToSend = htonl(cols), threadsCountToSend = htonl(threadsCount);
//	client.SendConfigurationData(rowsToSend, colsToSend, threadsCountToSend, rows, cols, data);
//	/*client.SendConfigurationData(rows, cols, threadsCount, data);*/
//	std::cout << "Data configuration has been sent" << '\n';
//	//std::this_thread::sleep_for(std::chrono::seconds(3));
//	response = client.ReceiveResponse();
//	if (response != "ACK") {
//		std::cerr << "ACK has not been sent from the server." << '\n';
//		delete[] data;
//		return 1;
//	}
//	std::cout << "Received ACK for Config" << '\n';
//	client.sendStartCommand();
//	std::cout << "Start command has been sent" << '\n';
//	//std::this_thread::sleep_for(std::chrono::seconds(3));
//	response = client.ReceiveResponse();
//	if (response != "STARTING") {
//		std::cerr << "Starting has not been sent from the server." << '\n';
//	}
//	std::cout << "Received Starting response" << '\n';
//	std::this_thread::sleep_for(std::chrono::seconds(3));
//	response = client.GetResult();
//
//	std::cout << "The sum is: " << response << '\n';
//	delete[] data;
//	return 0;
//}