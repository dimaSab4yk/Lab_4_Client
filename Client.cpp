#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
using namespace std;
using namespace std::chrono;

const int PORT = 8000;
const char* SERVER_IP = "127.0.0.1";

void fillMatrix(vector<vector<int>>& matrix, int n)
{
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            matrix[i][j] = rand() % 1000;
        }
    }                
}

void sendInt(SOCKET sock, int value)
{
    int netVal = htonl(value);
    send(sock, (char*)&netVal, sizeof(netVal), 0);
}

int recvInt(SOCKET sock)
{
    int netVal;
    recv(sock, (char*)&netVal, sizeof(netVal), 0);
    return ntohl(netVal);
}

void sendCommand(SOCKET sock, const char* cmd)
{
    char buffer[16] = {};
    strncpy_s(buffer, cmd, sizeof(buffer) - 1);
    send(sock, buffer, sizeof(buffer), 0);
}


bool processWithThreads(int n, int numThreads, const vector<vector<int>>& originalMatrix)
{
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed!" << endl;
        return false;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "Connection failed!" << endl;
        closesocket(clientSocket);
        return false;
    }

    sendCommand(clientSocket, "HELLO");

    sendCommand(clientSocket, "SEND_DATA");
    sendInt(clientSocket, n);
    sendInt(clientSocket, numThreads);

    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            sendInt(clientSocket, originalMatrix[i][j]);
        }
    }      
            
    sendCommand(clientSocket, "START_COMPUTATION");

    sendCommand(clientSocket, "GET_STATUS");

    sendCommand(clientSocket, "GET_RESULT");

    vector<vector<int>> mirroredMatrix(n, vector<int>(n));

    auto start = high_resolution_clock::now();

    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            mirroredMatrix[i][j] = recvInt(clientSocket);
        }
    }       
            
    auto end = high_resolution_clock::now();
    double seconds = duration_cast<nanoseconds>(end - start).count() * 1e-9;
    recv(clientSocket, (char*)&seconds, sizeof(seconds), 0);

    cout << "Threads: " << numThreads << " - Time: " << fixed << seconds << " seconds\n";

    closesocket(clientSocket);
    return true;
}

int main()
{
    srand(time(0));
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (wsaResult != 0)
    {
        cerr << "WSAStartup failed with error: " << wsaResult << endl;
        return 1;
    }

    int n = 0;
    cout << "Enter the size of the array: ";
    cin >> n;

    vector<vector<int>> matrix(n, vector<int>(n));
    fillMatrix(matrix, n);

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (clientSocket == INVALID_SOCKET)
    {
        cerr << "Socket creation failed!" << endl;
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        cerr << "Connection failed!" << endl;
        closesocket(clientSocket);
        return 1;
    }

    sendCommand(clientSocket, "HELLO");

    vector<int> threadCounts = { 1, 2, 4, 8, 16, 32, 64, 128, 256 };

    for (int numThreads : threadCounts)
    {
        sendCommand(clientSocket, "SEND_DATA");
        sendInt(clientSocket, n);
        sendInt(clientSocket, numThreads);

        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                sendInt(clientSocket, matrix[i][j]);
            }
        }                         

        sendCommand(clientSocket, "START_COMPUTATION");
        sendCommand(clientSocket, "GET_STATUS");
        sendCommand(clientSocket, "GET_RESULT");

        int dummyTimeNs = recvInt(clientSocket);
        vector<vector<int>> mirroredMatrix(n, vector<int>(n));

        auto start = high_resolution_clock::now();

        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                mirroredMatrix[i][j] = recvInt(clientSocket);
            }
        }            
                
        auto end = high_resolution_clock::now();
        double seconds = duration_cast<nanoseconds>(end - start).count() * 1e-9;

        cout << "Threads: " << numThreads << " - Time: " << fixed << seconds << " seconds\n";
    }

    sendCommand(clientSocket, "EXIT");
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
