#include <iostream>
#include <winsock2.h>
#include <thread>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

class Client {
private:
    SOCKET clientSocket = INVALID_SOCKET;

    void sendString(const string &str) const {
        int size = (int) str.size();
        send(clientSocket, (char *) &size, sizeof(int), 0);
        send(clientSocket, str.c_str(), size, 0);
    }

    void signInOrUp(int choice, const string &login, const string &password) const {
        send(clientSocket, (char *) &choice, sizeof(int), 0);

        send(clientSocket, login.c_str(), sizeof(login.size()), 0);
        send(clientSocket, password.c_str(), sizeof(password.size()), 0);
    }

    static void askForCredentials(string &name, string &password) {
        while (true) {
            cout << "Enter your login (max symbols 20): ";
            cin >> name;
            cin.get();

            if (name.size() > 20) {
                cerr << "Login error." << endl;
                continue;
            }

            cout << "Enter your password (max symbols 32): ";
            cin >> password;
            cin.get();

            if (password.size() > 32) {
                cerr << "Password error." << endl;
                continue;
            }

            break;
        }
    }

    void sendMessage(int choice) {
        send(clientSocket, (char *) &choice, sizeof(int), 0);

        string username, message;

        cout << "Enter name of user: ";
        cin >> username;
        cin.get();

        send(clientSocket, username.c_str(), sizeof(username.size()), 0);

        cout << "Enter message: ";
        getline(cin, message);

        sendString(message);

        Sleep(500);
    }

    void checkOnline(int choice) const {
        send(clientSocket, (char *) &choice, sizeof(int), 0);
        cout << "Online: " << endl;
        Sleep(500);
    }

public:
    Client() {
        // Инициализация Winsock
        WSADATA wsaData = {0};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cerr << "Failed to initialize Winsock\n";
            exit(-1);
        }

        // Создание сокета
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Failed to create socket\n";
            WSACleanup();
            exit(-1);
        }
    }

    void connectToServer(const string &serverAddress, const int port) const {
        // Настройка адреса сервера
        sockaddr_in serverAddr = {0};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        serverAddr.sin_addr.s_addr = inet_addr(serverAddress.c_str());

        // Соединение с сервером
        if (connect(clientSocket, (sockaddr *) &serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            cerr << "Failed to connect to server\n";
            closesocket(clientSocket);
            WSACleanup();
            exit(-1);
        }
    }

    void logUser() const {
        int choice;
        cout << "Sign in - 1 or Sign up - 2: ";
        cin >> choice;
        cin.get();

        string login, password;

        askForCredentials(login, password);

        if (choice == 1 || choice == 2) {
            signInOrUp(choice, login, password);
        } else {
            cerr << "Choice error";
            exit(-1);
        }
    }

    void runLoop() {
        int choice;
        while (true) {
            cout << "Send message - 1, Checking online - 2, Exit - 3: ";
            cin >> choice;
            cin.get();
            if (choice == 1) {
                sendMessage(choice);
            } else if (choice == 2) {
                checkOnline(choice);
            } else if (choice == 3) {
                cout << "Closing program..." << endl;
                exit(0);
            } else {
                cerr << "Choice error";
                exit(-1);
            }
        }
    }

    static void receiveMessages(const Client &client) {
        int msg_size;
        SOCKET clientSocket = client.getSocket();

        while (true) {
            int bytesReceived = recv(clientSocket, (char *) &msg_size, sizeof(int), 0);

            if (bytesReceived == SOCKET_ERROR || bytesReceived == 0) {
                cout << "Server closed connection" << endl;
                exit(-1);
            }

            char *msg = new char[msg_size + 1];
            msg[msg_size] = '\0';
            recv(clientSocket, msg, msg_size, 0);
            cout << msg << endl;

            string msgE(msg); // Проверка на ошибки с сервера
            if (msgE == "Account doesnt exist") {
                cout << "Invalid credentials. Try enter again";
                exit(-1);
            }
            if (msgE == "Account already exist") {
                cout << "Try enter again";
                exit(-1);
            }

            delete[] msg;
        }
    }

    ~Client() {
        // Закрытие сокета и выход
        closesocket(clientSocket);
        WSACleanup();
    }

    SOCKET getSocket() const {
        return clientSocket;
    }
};

int main() {
    Client client;
    client.connectToServer("127.0.0.1", 5555);
    client.logUser();

    thread(Client::receiveMessages, client).detach();
    Sleep(500);

    client.runLoop();
    return 0;
}