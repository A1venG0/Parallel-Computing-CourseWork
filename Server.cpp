#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <mutex>
#include <vector>
#include <shared_mutex>
#include <future>


class Server
{
public:
	Server(int port) : m_port(port), m_listenSocket(INVALID_SOCKET),
		m_running(false) {};

	Server(const Server& other) : m_port(other.m_port),
		m_listenSocket(other.m_listenSocket),
		m_running(other.m_running) {};
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
		AcceptConnections();

		return true;
	}

	void Stop()
	{
		std::cout << "In the stop" << '\n';
		if (m_running)
		{
			m_running = false;
			{
				std::unique_lock<std::shared_mutex> lock(m_clientsMutex);
				for (auto& clientThread : m_clientThreads)
					clientThread.join();
				for (auto& client : m_clients) {
					delete[] client->datat;
				}
			}
			closesocket(m_listenSocket);
			WSACleanup();
		}
	}
	enum class TaskState
	{
		Idle,
		Running,
		Completed
	};

private:

	struct ClientInfo
	{
		SOCKET clientSocket;
		int rows;
		int cols;
		int* datat;
		Server::TaskState taskState;
		std::thread taskThread;
		std::promise<int> promise;
		std::future<int> future;
		bool running;

		ClientInfo(const ClientInfo& other) : clientSocket(other.clientSocket),
			rows(other.rows), cols(other.cols), datat(other.datat), taskState(other.taskState),
			running(other.running) {};
		ClientInfo() : clientSocket(INVALID_SOCKET), rows(0), cols(0), datat(nullptr),
			taskState(TaskState::Idle), running(true) {};
	};

	void AcceptConnections()
	{
		while (m_running)
		{
			SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
			if (clientSocket == INVALID_SOCKET) {
				std::cerr << "accept failed: " << WSAGetLastError() << '\n';
				continue;
			}

			auto client = std::make_shared<ClientInfo>();
			client->clientSocket = clientSocket;
			client->taskState = TaskState::Idle;

			{
				std::lock_guard<std::shared_mutex> lock(m_clientsMutex);
				m_clients.push_back(client);
			}

			// Create a new thread for the client connection
			std::thread clientThread(&Server::ClientThreadFunction, this, client);
			m_clientThreads.push_back(std::move(clientThread));
		}
	}

	void ClientThreadFunction(std::shared_ptr<ClientInfo> client)
	{
		while (m_running && client->running)
		{
			std::string command;
			if (!ReceiveCommand(client->clientSocket, command))
			{
				//client->running = false; // not sure about this one
				//closesocket(client->clientSocket);
				continue;
			}

			if (command == "INI")
			{
				SendResponse(client->clientSocket, "ACK");
				if (!ReceiveData(*client))
				{
					//client->running = false; // not sure about this one
					//closesocket(client->clientSocket);
					continue;
				}
				m_outputMutex.lock();
				std::cout << "Data configuration has been received" << '\n';
				
				for (int i = 0; i < client->rows; i++)
				{
					for (int j = 0; j < client->cols; j++)
					{
						std::cout << client->datat[i * client->cols + j] << ' ';
					}
				}
				m_outputMutex.unlock();
				std::cout << '\n';
				SendResponse(client->clientSocket, "ACK");
			}
			else if (command == "STA")
			{
				m_outputMutex.lock();
				std::cout << "Start command has been received" << '\n';
				m_outputMutex.unlock();
				SendResponse(client->clientSocket, "STARTING");

				std::unique_lock<std::mutex> lock(m_taskMutex);

				if (client->future.valid())
				{
					SendResponse(client->clientSocket, "ERROR: Task already running");
				}
				else
				{
					std::promise<int> promise;
					client->promise = std::move(promise);
					client->future = client->promise.get_future();

					client->taskState = TaskState::Running;
					client->taskThread = std::thread(&Server::TaskThreadFunction, this, client);
					client->taskThread.join();
				}
			}
			else if (command == "GET")
			{
				std::string stateStr;
				std::string result;
				
				if (!client->future.valid())
					stateStr = "IDLE";
				else if (client->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					stateStr = "COMPLETED";
				}
				else
				{
					stateStr = "RUNNING";
				}
				result = "STATE:" + stateStr;
				if (stateStr == "COMPLETED")
					result += " RESULT:" + std::to_string(client->future.get());
				
				m_outputMutex.lock();
				std::cout << "Sending the result" << '\n';
				m_outputMutex.unlock();
				SendResponse(client->clientSocket, result);
				client->running = false; // not sure about this one
				closesocket(client->clientSocket);
			}
			
			//closesocket(clientSocket);
		}
		
		{
			// Remove the client from the list
			std::lock_guard<std::shared_mutex> lock(m_clientsMutex);
			m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client), m_clients.end());
		}
		
	}

	void TaskThreadFunction(std::shared_ptr<ClientInfo> client)
	{
		int sum = 0;
		for (int i = 0; i < client->rows; i++)
		{
			for (int j = 0; j < client->cols; j++)
			{
				sum += client->datat[i * client->cols + j];
			}
		}

		client->taskState = TaskState::Completed;

		client->promise.set_value(sum);
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

	bool ReceiveData(ClientInfo& client)
	{
		/*std::string response;
		if (!ReceiveResponse(socket, response)) {
			return false;
		}*/

		if (recv(client.clientSocket, (char*)&client.rows, sizeof(int), 0) != sizeof(int) ||
			recv(client.clientSocket, (char*)&client.cols, sizeof(int), 0) != sizeof(int))
		{
			std::cerr << "Receive matrix size failed: " << WSAGetLastError() << '\n';
			return false;
		}

		client.datat = new int[client.rows * client.cols];

		if (recv(client.clientSocket, (char*)client.datat, sizeof(int) * client.rows * client.cols, 0) == SOCKET_ERROR)
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


	bool SendResponse(SOCKET socket, const std::string& response)
	{
		if (send(socket, response.c_str(), response.size(), 0) == SOCKET_ERROR)
		{
			std::cerr << "SendResponse failed: " << WSAGetLastError() << '\n';
			return false;
		}

		return true;
	}


	SOCKET m_listenSocket;
	bool m_running;
	std::mutex m_taskMutex;
	std::shared_mutex m_clientsMutex;
	std::mutex m_outputMutex;
	std::vector<std::shared_ptr<ClientInfo>> m_clients;
	std::vector<std::thread> m_clientThreads;
	int m_port;
};

int main()
{
	auto server = Server(1234);
	server.Start();

	std::cout << "Press enter to stop the server" << std::endl;
	std::cin.get();

	server.Stop();
	return 0;
}