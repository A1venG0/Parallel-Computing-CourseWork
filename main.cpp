#include <vector>
#include <iostream>
#include <chrono>
#include <random>

#include "ThreadPool.h"
#include "TaskQueue.h"
#include "Server.cpp"


int main()
{
    auto server = Server(1234);
    server.Start();
    
    std::cout << "Press enter to stop the server" << std::endl;
    std::cin.get();
    
    server.Stop();
    return 0;
}