#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 25565
#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 16
#define MAX_CLIENTS 10
#define PACKAGE_HEADER "msg"
#define PACKAGE_HEADER_LENGTH 3

struct Client 
{
    int socket;
    struct sockaddr_in address;
    char username[MAX_USERNAME_LENGTH + 1];
};

struct Client clients[MAX_CLIENTS + 1];
pthread_t threads[MAX_CLIENTS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int numClients = 0;

void setup()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
		clients[i].socket = -1;

    sprintf(clients[MAX_CLIENTS].username, "SERVER");
}

void broadcastMessage(char *message, int senderIndex)
{
    pthread_mutex_lock(&mutex);
    char messageWithUsername[BUFFER_SIZE + MAX_USERNAME_LENGTH + 4];
    sprintf(messageWithUsername, "[%s] %s", clients[senderIndex].username, message);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].socket != -1 && i != senderIndex)
            send(clients[i].socket, messageWithUsername, strlen(messageWithUsername), 0);
    }

    pthread_mutex_unlock(&mutex);
}

int checkMessage(char* message, int length)
{
    if(length <= 3)
        return 0;

    if(message[0] != 'm' || message[1] != 's' || message[2] != 'g')
        return 0;

    for(int i = 0; i < length - 3; i++) {
        message[i] = message[i + 3];
    }

    message[length - 3] = '\0';

    if(length >= 4 && message[length - 4] == '\n')
        message[length - 4] = '\0';

    return 1;   
}

void* handleClient(void* arg)
{
    int index = *(int*)arg;
    int clientSocket = clients[index].socket;
    int usernameLength = recv(clientSocket, clients[index].username, MAX_USERNAME_LENGTH + PACKAGE_HEADER_LENGTH, 0);
    int gotUserName = 0;
    char buffer[BUFFER_SIZE];

    if(checkMessage(clients[index].username, usernameLength))
    {
        clients[index].username[usernameLength] = '\0';
        printf("[CONNECTION] Client %d connected with username %s\n", index, clients[index].username);
        sprintf(buffer, "%s joined the chat", clients[index].username);
        broadcastMessage(buffer, MAX_CLIENTS);
        gotUserName = 1;
    }
    else
    {
        fprintf(stderr, "[CONNECTION] Failed to receive username from client %d - REJECTED\n", index);
    }

    if(gotUserName)
    {
        while (1)
        {
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0 && checkMessage(buffer, bytesReceived))
            {
                buffer[bytesReceived] = '\0';
                printf("Received message from %s: %s\n", clients[index].username, buffer);
                broadcastMessage(buffer, index);

            }
            else if (bytesReceived == 0)
            {
                printf("[CONNECTION] Client %s disconnected\n", clients[index].username);
                break;
            }
            else
            {
                perror("[CONNECTION] Error receiving data");
                break;
            }
        }

        //clients[index].socket = -1;
        //sprintf(buffer, "%s left the chat\n", clients[index].username);
        //broadcastMessage(buffer, MAX_CLIENTS);
    }

    pthread_mutex_lock(&mutex);
    close(clientSocket);
    clients[index].socket = -1;
    numClients--;
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

int main() 
{
	setup();
    int serverSocket;
    struct sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);

    // Tworzenie gniazda serwera
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Ustawienie opcji keep-alive
    int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0)
    {
        perror("Error setting keep-alive option");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Konfiguracja adresu serwera
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Przypisanie adresu do gniazda serwera
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Error binding socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Nasłuchiwanie na gnieździe serwera
    if (listen(serverSocket, MAX_CLIENTS) == -1)
    {
        perror("Error listening on socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    while (1)
    {
        int clientSocket = accept(serverSocket, (struct sockaddr *)&serverAddr, &addrLen);

        if (clientSocket == -1)
        {
            perror("[CONNECTION] Error accepting connection");
            close(serverSocket);
            exit(EXIT_FAILURE);
        }

        printf("[CONNECTION] New connection detected.\n");

        pthread_mutex_lock(&mutex);
        if (numClients < MAX_CLIENTS)
        {
            for (int i = 0; i < MAX_CLIENTS; ++i)
            {
                if (clients[i].socket == -1)
                {
                    clients[i].socket = clientSocket;
                    clients[i].address = serverAddr;

                    // Tworzymy nowy wątek do obsługi klienta
                    pthread_create(&threads[i], NULL, handleClient, (void *)&i);
                    numClients++;
                    break;
                }
            }
        } 
        else
        {
            fprintf(stderr, "[CONNECTION] Too many clients. Connection rejected.\n");
            close(clientSocket);
        }
        pthread_mutex_unlock(&mutex);
    }

    close(serverSocket);
    return 0;
}
