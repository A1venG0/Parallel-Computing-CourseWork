#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <mutex>
#include <vector>
#include <shared_mutex>
#include <future>
#include <filesystem>
#include <fstream>
#include "ThreadPool.h"
#include "InvertedIndex.h"


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

		//threadPool = new ThreadPool();

		threadPool.initilize(NUMBER_OF_THREADS);

		int fileCount = 0;
		for (int i = 1; i <= 5; i++)
		{
			LoadInvertedIndexWithFiles("dataset\\" + std::to_string(1) + "\\", fileCount);
		}
		//threadPool.terminate();
		//std::this_thread::sleep_for(std::chrono::seconds(10));
		m_running = true;

		/*std::unordered_set<int> docs;
		threadPool.add_task([this, &docs]()
			{
				this->invertedIndex.Search("Sample", docs);
			});*/
		//invertedIndex.Search("Sample", docs);
		/*for (auto& doc : docs)
		{
			std::cout << doc << ' ';
		}

		std::cout << '\n';*/
		//delete threadPool;

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
				threadPool.terminate();
				for (auto& clientThread : m_clientThreads)
					clientThread.join();
			}
			closesocket(m_listenSocket);
			WSACleanup();
		}
	}

	~Server()
	{
		//delete threadPool;
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
		std::string query;
		std::unordered_set<int> result;
		Server::TaskState taskState;
		std::thread taskThread;
		/*std::promise<int> promise;*/
		std::future<int> future;
		bool running;

		ClientInfo(const ClientInfo& other) : clientSocket(other.clientSocket),
			query(other.query), taskState(other.taskState), running(other.running) {};
		ClientInfo() : clientSocket(INVALID_SOCKET), query(""),
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
				std::cout << "Data configuration has been received: " << client->query << '\n';
				m_outputMutex.unlock();
				
				SendResponse(client->clientSocket, "ACK");
			}
			else if (command == "STA")
			{
				m_outputMutex.lock();
				std::cout << "Start command has been received" << '\n';
				m_outputMutex.unlock();
				SendResponse(client->clientSocket, "STARTING");

				std::unique_lock<std::mutex> lock(m_taskMutex);

				//if (client->future.valid())
				if (client->taskState == TaskState::Running)
				{
					SendResponse(client->clientSocket, "ERROR: Task already running");
				}
				else
				{
					/*std::promise<int> promise;
					client->promise = std::move(promise);*/
					/*client->future = client->promise.get_future();*/

					client->taskState = TaskState::Running;
					threadPool.add_task([this, client]() { this->invertedIndex.Search(client->query, client->result); }); // stopped here (incorrect state)
					//client->future = std::async(std::launch::async, &Server::TaskThreadFunction, this, client);
					/*client->taskThread = std::thread(&Server::TaskThreadFunction, this, client);
					client->taskThread.join();*/
				}
			}
			else if (command == "GET")
			{
				std::string stateStr;
				std::string result;
				
				if (client->taskState == TaskState::Idle)
					stateStr = "IDLE";
				else if (client->taskState == TaskState::Completed)
					stateStr = "COMPLETED";
				else
					stateStr = "RUNNING";
				/*if (!client->future.valid())
					stateStr = "IDLE";
				else if (client->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				{
					stateStr = "COMPLETED";
				}
				else
				{
					stateStr = "RUNNING";
				}*/
				result = "STATE:" + stateStr;
				if (stateStr == "COMPLETED")
				{
					result += " RESULT:";
					for (auto it = client->result.begin(); it != client->result.end(); it++)
						result += " " + *it;
				}
					
				
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

	//int TaskThreadFunction(std::shared_ptr<ClientInfo> client)
	//{
	//	std::vector<std::thread> workers(client->threadsNumber);
	//	int rowsPerThread = client->rows / client->threadsNumber;
	//	int sum = 0;
	//	
	//	for (int i = 0; i < client->threadsNumber; i++)
	//	{
	//		int startRow = i * rowsPerThread;
	//		int endRow = (i == client->threadsNumber - 1) ? client->rows : (i + 1) * rowsPerThread;
	//		int threadSum = 0;

	//		workers[i] = std::thread(&Server::calculateSum, this, startRow, endRow, client, threadSum, std::ref(sum));
	//	}
	//	for (int i = 0; i < client->threadsNumber; i++)
	//		workers[i].join();

	//	client->taskState = TaskState::Completed;
	//	/*client->promise.set_value(sum);*/
	//	return sum;
	//}

	/*void calculateSum(const int startRow, const int endRow, std::shared_ptr<ClientInfo> client, int threadSum, int& sum) {

		for (int i = startRow; i < endRow; i++) {
			for (int j = 0; j < client->cols; j++) {
				threadSum += client->datat[i * client->cols + j];
			}
		}

		m_sumMutex.lock();
		sum += threadSum;
		m_sumMutex.unlock();

	}*/

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
		int receivedQueryLength;
		if (recv(client.clientSocket, (char*)&receivedQueryLength, sizeof(int), 0) != sizeof(int))
		{
			std::cerr << "Receive query length failed: " << WSAGetLastError() << '\n';
			return false;
		}

		int newReceivedQueryLength = ntohl(receivedQueryLength);
		char* buffer = new char[newReceivedQueryLength];

		if (recv(client.clientSocket, buffer, sizeof(char) * newReceivedQueryLength, 0) == SOCKET_ERROR)
		{
			std::cerr << "Receive query failed: " << WSAGetLastError() << '\n';
			delete[] buffer;
			return false;
		}

		client.query = std::string(buffer, newReceivedQueryLength);
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

	void LoadInvertedIndexWithFiles(const std::string& directoryPath, int& fileCount)
	{
		for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
			if (entry.is_regular_file()) {
				std::ifstream file(entry.path());
				fileCount++;
				if (file.is_open()) {
					std::string line;
					while (std::getline(file, line)) {
						threadPool.add_task([this, line, fileCount]() {
							this->invertedIndex.AddDocument(line, fileCount);
							});
					}

					file.close();
				}
				else {
					std::cerr << "Unable to open file: " << entry.path().filename() << '\n';
				}
			}
		}
	}


	SOCKET m_listenSocket;
	ThreadPool threadPool;
	bool m_running;
	std::mutex m_taskMutex;
	std::mutex m_sumMutex;
	std::shared_mutex m_clientsMutex;
	std::mutex m_outputMutex;
	std::vector<std::shared_ptr<ClientInfo>> m_clients;
	std::vector<std::thread> m_clientThreads;
	InvertedIndex invertedIndex;
	int m_port;
	const int NUMBER_OF_THREADS = 4;
};

//int main()
//{
//	auto server = Server(1234);
//	server.Start();
//
//	std::cout << "Press enter to stop the server" << std::endl;
//	std::cin.get();
//
//	server.Stop();
//	return 0;
//}