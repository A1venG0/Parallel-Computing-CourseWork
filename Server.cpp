#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <mutex>


class Server
{
public:
	Server(int port) : m_port(port), m_listenSocket(INVALID_SOCKET),
		m_running(false), m_cols(0), m_rows(0), m_result(0),
		m_taskState(TaskState::Idle), m_data(nullptr) {};

	Server(const Server& other) : m_port(other.m_port),
		m_listenSocket(other.m_listenSocket),
		m_running(other.m_running),
		m_cols(other.m_cols),
		m_rows(other.m_rows),
		m_result(other.m_result),
		m_taskState(other.m_taskState)
	{};
	bool Start()
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			std::cerr << "WSAStartup failed: " << WSAGetLastError() << '\n';
			return false;
		}

		m_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (m_listenSocket == INVALID_SOCKET) {
			std::cerr << "socket failed: " << WSAGetLastError() << '\n';
			WSACleanup();
			return false;
		}

		sockaddr_in localAddress;
		localAddress.sin_family = AF_INET;
		localAddress.sin_addr.s_addr = INADDR_ANY;
		localAddress.sin_port = htons(m_port);
		if (bind(m_listenSocket, (sockaddr*)&localAddress, sizeof(localAddress)) == SOCKET_ERROR)
		{
			std::cerr << "bind failed: " << WSAGetLastError() << '\n';
				closesocket(m_listenSocket);
				WSACleanup();
				return false;
		}

		if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::cerr << "listen failed: " << WSAGetLastError() << '\n';
			closesocket(m_listenSocket);
			WSACleanup();
			return false;
		}

		m_running = true;
		m_clientThread = std::thread(&Server::ClientThreadFunction, this);

		return true;
	}

	void Stop()
	{
		if (m_running)
		{
			m_running = false;
			m_clientThread.join();
			m_taskThread.join();
			delete[] m_data;
			closesocket(m_listenSocket);
			WSACleanup();
		}
	}

private:

	void ClientThreadFunction()
	{
		SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET) {
			std::cerr << "accept failed: " << WSAGetLastError() << '\n';
			closesocket(m_listenSocket);
			return;
		}
		while (m_running)
		{
			std::string command;
			if (!ReceiveCommand(clientSocket, command))
			{
				//closesocket(clientSocket);
				continue;
			}

			if (command == "INI")
			{
				SendResponse(clientSocket, "ACK");
				int rows, cols;
				if (!ReceiveData(clientSocket, rows, cols))
				{
					//closesocket(clientSocket);
					continue;
				}
				std::cout << "Data configuration has been received" << '\n';
				for (int i = 0; i < rows; i++)
				{
					for (int j = 0; j < cols; j++)
					{
						std::cout << m_data[i * cols + j] << ' ';
					}
				}
				std::cout << '\n';
				SendResponse(clientSocket, "ACK");
				m_rows = rows;
				m_cols = cols;
			}
			else if (command == "STA")
			{
				std::cout << "Start command has been received" << '\n';
				SendResponse(clientSocket, "STARTING");

				std::unique_lock<std::mutex> lock(m_taskMutex);

				if (m_taskState != TaskState::Idle)
				{
					SendResponse(clientSocket, "ERROR: Task already running");
				}
				else
				{
					m_taskState = TaskState::Running;

					m_taskThread = std::thread(&Server::TaskThreadFunction, this);
				}
			}
			else if (command == "GET")
			{
				std::string stateStr;
				switch (m_taskState)
				{
					case TaskState::Idle:
						stateStr = "IDLE";
						break;
					case TaskState::Running:
						stateStr = "RUNNING";
						break;
					case TaskState::Completed:
						stateStr = "COMPLETED";
						break;
				}
				std::string result = "STATE:" + stateStr;

				if (m_taskState == TaskState::Completed) {
					result += " RESULT:" + std::to_string(m_result);
					
				}
				SendResponse(clientSocket, result);
			}

			//closesocket(clientSocket);
		}
	}

	void TaskThreadFunction()
	{
		int sum = 0;
		for (int i = 0; i < m_rows; i++)
		{
			for (int j = 0; j < m_cols; j++)
			{
				sum += m_data[i * m_cols + j];
			}
		}

		{
			std::unique_lock<std::mutex> lock(m_taskMutex);
			m_taskState = TaskState::Completed;
			m_result = sum;
		}
	}

	bool ReceiveCommand(SOCKET socket, std::string& command)
	{
		char buffer[1024];
		ZeroMemory(buffer, 1024);
		int result = recv(socket, buffer, 1024, 0);
		if (result == SOCKET_ERROR)
		{
			//std::cerr << "ReceiveCommand failed: " << WSAGetLastError() << '\n';
			return false;
		}
		buffer[result] = '\0';
		command = std::string(buffer, result);

		return true;
	}

	bool ReceiveData(SOCKET socket, int& rows, int& cols)
	{
		/*std::string response;
		if (!ReceiveResponse(socket, response)) {
			return false;
		}*/

		if (recv(socket, (char*)&rows, sizeof(int), 0) != sizeof(int) ||
			recv(socket, (char*)&cols, sizeof(int), 0) != sizeof(int))
		{
			std::cerr << "Receive matrix size failed: " << WSAGetLastError() << '\n';
			return false;
		}

		m_data = new int[rows * cols];

		if (recv(socket, (char*)m_data, sizeof(int) * rows * cols, 0) == SOCKET_ERROR)
		{
			std::cerr << "Receive matrix failed: " << WSAGetLastError() << '\n';
			return false;
		}

		/*data = new int* [rows];

		for (int i = 0; i < rows; i++)
		{
			data[i] = new int[cols];
			for (int j = 0; j < cols; j++)
			{
				data[i][j] = buffer[i * cols + j];
			}
		}

		delete[] buffer;*/
		return true;
	}

	bool ReceiveResponse(SOCKET socket, std::string& response)
	{
		char buffer[1024];
		int result = recv(socket, buffer, 1024, 0);

		if (result == SOCKET_ERROR)
		{
			std::cerr << "Recieve response failed: " << WSAGetLastError() << '\n';
			return false;
		}
		buffer[result] = '\0';
		response = std::string(buffer);
		return true;
	}

	bool SendResponse(SOCKET socket, const std::string& response)
	{
		if (send(socket, response.c_str(), response.size(), 0) == SOCKET_ERROR)
		{
			std::cerr << "SendResponse failed: " << WSAGetLastError() << '\n';
			return false;
		}

		return true;
	}

	enum class TaskState
	{
		Idle,
		Running,
		Completed
	};

	SOCKET m_listenSocket;
	bool m_running;
	std::thread m_clientThread;
	std::thread m_taskThread;
	//std::unique_ptr<int[]> m_data;
	int* m_data;
	int m_rows;
	int m_cols;
	int m_result;
	std::mutex m_taskMutex;
	TaskState m_taskState;
	/*SOCKET m_serverSocket;
	SOCKET m_clientSocket;
	sockaddr_in m_serverAddr;
	sockaddr_in m_clientAddr;*/
	int m_port;
};

int main()
{
	auto server = Server(1234);
	server.Start();

	std::cout << "Press enter to stop the server" << std::endl;
	std::cin.get();

	server.Stop();

}