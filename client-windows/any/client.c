#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <winsock2.h>
#include <process.h>

#define SERVER_IP "127.0.0.1"
#define PORT 25565
#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 16
#define PACKAGE_HEADER "msg"
#define PACKAGE_HEADER_LENGTH 3

void receiveMessages(void* clientSocket) {
    SOCKET socket = *((SOCKET*)clientSocket);
    char buffer[BUFFER_SIZE];
    int bytesReceived;

    while ((bytesReceived = recv(socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        printf("\r%s\nYou: ", buffer);
    }

    if (bytesReceived == 0) {
        printf("Server disconnected\n");
    }
    else {
        fprintf(stderr, "Recv failed with error: %d\n", WSAGetLastError());
    }

    _endthread();
}

int sendPackageToServer(SOCKET* clientSocket, const char* content, int contentLength)
{
    char newContent[BUFFER_SIZE];
    sprintf(newContent, "%s%s", PACKAGE_HEADER, content);
    
    if (send(clientSocket, newContent, contentLength + PACKAGE_HEADER_LENGTH, 0) == SOCKET_ERROR) {
        fprintf(stderr, "Send failed with error: %d\n", WSAGetLastError());
        return 0;
    }

    return 1;
}


int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Failed to initialize winsock\n");
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create socket\n");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    serverAddr.sin_port = htons(PORT);

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        fprintf(stderr, "Connection failed with error: %d\n", WSAGetLastError());
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    printf("Connected to the server\n");

    // Wysy�anie nazwy u�ytkownika do serwera
    char username[MAX_USERNAME_LENGTH + 1];  // +1 na znak null-terminatora
    printf("Enter your username (max 16 characters): ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';  // Usuwa znak nowej linii z nazwy u�ytkownika

    if (!sendPackageToServer(clientSocket, username, strlen(username)))
        return 1;

    // W�tek do odbierania wiadomo�ci
    _beginthread(receiveMessages, 0, &clientSocket);

    char message[BUFFER_SIZE];

    while (1) {
        printf("You: ");
        fgets(message, BUFFER_SIZE, stdin);

        if (!sendPackageToServer(clientSocket, message, strlen(message)))
            return 1;
    }

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}