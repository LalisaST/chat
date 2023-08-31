#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

unordered_map<string, SOCKET> clients;

class Client {
private:
    SOCKET clientSocket = INVALID_SOCKET;
    string login, password;

    void sendString(const string &str) const {
        int size = (int) str.size();
        send(clientSocket, (char *) &size, sizeof(int), 0);
        send(clientSocket, str.c_str(), size, 0);
    }

    bool checkIfExists() {
        bool flag = false;
        string log, pass;
        ifstream in_file_f("clients.txt", ios::in);
        if (in_file_f.is_open()) { // проверить, удалось ли открыть файл
            while (in_file_f >> log && in_file_f >> pass) {
                if (login == log && password == pass) {
                    flag = true;
                    break;
                }
            }
            in_file_f.close(); // закрыть файл

            return flag;
        } else {
            cerr << "Error: Failed to open file." << std::endl;
            return false;
        }
    }

public:
    Client(SOCKET clientSocket, const string &login, const string &password) {
        this->clientSocket = clientSocket;
        this->login = login;
        this->password = password;
    }


    bool checkSignInOrUp(int choice) {
        if (choice == 1) {
            if (!checkIfExists()) {
                string error = "Account doesnt exist";
                sendString(error);
                closesocket(clientSocket);
                return false;
            }
        } else if (choice == 2) {
            //Проверка на наличие аккаунта
            if (checkIfExists()) {
                string error = "Account already exist";
                sendString(error);
                closesocket(clientSocket);
                return false;
            }

            ofstream file("clients.txt", ios_base::app); // открыть файл для записи с дозаписью

            if (file.is_open()) { // проверить, удалось ли открыть файл
                file << login << ' ' << password << endl; // записать строку в конец файла
                file.close(); // закрыть файл
            } else {
                cerr << "Error: Failed to open file." << std::endl;
                return false;
            }
        }

        return true;
    }
};

class Server {
private:
    SOCKET serverSocket = INVALID_SOCKET;
    int portNumber = 5555;

    static void acceptClient(SOCKET clientSocket, sockaddr_in clientAddr) {
        int bytes_received;

        int choice;
        recv(clientSocket, (char *) &choice, sizeof(int), 0);

        // Принятия логина
        char *login = new char[21];
        bytes_received = recv(clientSocket, login, sizeof(login), 0);

        if (checkForErrors(bytes_received, clientAddr, login)) return;

        login[bytes_received] = '\0';
        cout << login << endl;

        // Принятия пароля
        char *password = new char[33];
        bytes_received = recv(clientSocket, password, sizeof(password), 0);

        password[bytes_received] = '\0';
        cout << password << endl;

        Client client(clientSocket, login, password);

        if (!client.checkSignInOrUp(choice)) {
            return;
        }

        clients[login] = clientSocket;
        cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port)
             << endl;

        waitForClient(login, clientAddr);
    }

    static void waitForClient(const string &login, sockaddr_in clientAddr) {
        int bytes_received;
        while (true) {
            int choiceAction;
            bytes_received = recv(clients[login], (char *) &choiceAction, sizeof(int), 0);
            if (checkForErrors(bytes_received, clientAddr, login)) return;
            //Отправить сообщение или проверить онлайн
            if (choiceAction == 1) {
                char *bufferName = new char[21];

                //принятие имени получателя
                bytes_received = recv(clients[login], bufferName, sizeof(bufferName), 0);

                cout << "Message to: "<<bufferName << endl;

                if (checkForErrors(bytes_received, clientAddr, login)) return;

                bufferName[bytes_received] = '\0';
                string username(bufferName);

                int msg_size;
                recv(clients[login], (char *) &msg_size, sizeof(int), 0);

                char *buffer = new char[msg_size + 1];
                recv(clients[login], buffer, sizeof(buffer), 0);

                buffer[msg_size] = '\0';
                cout << "Received from " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << ": "
                     << buffer << endl;

                sendMessageClient(buffer, login, username);
                delete[] buffer;

            } else if (choiceAction == 2) {
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    int msg_size = (int) it->first.size();
                    send(clients[login], (char *) &msg_size, sizeof(int), 0);
                    send(clients[login], it->first.c_str(), msg_size, 0);
                }
            }
        }
        closesocket(clients[login]);
    }

    static bool checkForErrors(int bytesReceived, sockaddr_in clientAddr, const string &login) {
        if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
            cout << "Client " << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port)
                 << " disconnected\n";
            clients.erase(login);
            return true;
        }
        return false;
    }

    static void sendMessageClient(char *message, const string &login, const string &username) {
        string error;

        if (!clients.count(username)) {
            error = "Username doesnt exist";
            sendString(error, clients[login]);
            return;
        }

        string sender = "\n";
        sender.append(login);
        sender.append(": ");
        sender.append(message);
        sender.append("\n");
        sendString(sender, clients[username]);
    }

    static void sendString(const string &str, SOCKET clientSocket) {
        int size = (int) str.size();
        send(clientSocket, (char *) &size, sizeof(int), 0);
        send(clientSocket, str.c_str(), size, 0);
    }

    void setupServer() const {
        // Настройка сокета
        sockaddr_in serverAddr = {0};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
        serverAddr.sin_port = htons(portNumber);

        BOOL reuseAddr = TRUE;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuseAddr, sizeof(reuseAddr)) ==
            SOCKET_ERROR) {
            cerr << "Failed to set socket options\n";
            closesocket(serverSocket);
            WSACleanup();
            exit(-1);
        }

        // Связывание адреса с сокетом
        if (bind(serverSocket, (sockaddr *) &serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Failed to bind socket\n";
            closesocket(serverSocket);
            WSACleanup();
            exit(-1);
        }
    }

public:
    Server(int portNumber) {
        this->portNumber = portNumber;
        // Инициализация Winsock
        WSADATA wsaData = {0};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "Failed to initialize Winsock\n";
            exit(-1);
        }

        // Создание сокета
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            cerr << "Failed to create socket\n";
            WSACleanup();
            exit(-1);
        }

        setupServer();
    }

    void startServer() const {
        // Запуск сервера
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            cerr << "Failed to listen to socket\n";
            closesocket(serverSocket);
            WSACleanup();
            exit(-1);
        }

        cout << "Server is running on port " << portNumber << endl;

        // Обработка подключений клиентов
        SOCKET clientSocket;
        sockaddr_in clientAddr{};
        int clientAddrSize = sizeof(clientAddr);

        while (true) {
            clientSocket = accept(serverSocket, (sockaddr *) &clientAddr, &clientAddrSize);
            if (clientSocket == INVALID_SOCKET) {
                cerr << "Failed to accept client socket\n";
                continue;
            }
            thread(acceptClient, clientSocket, clientAddr).detach();
        }
    }

    ~Server() {
        closesocket(serverSocket);
        WSACleanup();
    }
};


int main() {
    Server server(5555);
    server.startServer();
    return 0;
}