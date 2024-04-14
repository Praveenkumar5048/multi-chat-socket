#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h> // Include for bool data type

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

struct Client {
    int socket;
    char username[50];
    bool is_active; // Added field to track client status
};

struct Client clients[MAX_CLIENTS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void send_message_to_clients(int sender_socket, const char *message) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != -1 && clients[i].is_active && clients[i].socket != sender_socket) {
            char message_with_sender[100];
            snprintf(message_with_sender, sizeof(message_with_sender), "[%s]: %s", clients[sender_socket].username, message);
            send(clients[i].socket, message_with_sender, strlen(message_with_sender), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Prompt for username
    bytes_received = recv(client_socket, clients[client_socket].username, sizeof(clients[client_socket].username), 0);

    if (bytes_received <= 0) {
        close(client_socket);
        pthread_exit(NULL);
    }

    clients[client_socket].username[bytes_received] = '\0'; // Null-terminate the received data
    clients[client_socket].socket = client_socket;
    clients[client_socket].is_active = true;

    printf("%s got connected\n", clients[client_socket].username);

    while (1) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0 || strcmp(buffer, "exit") == 0) {
            printf("%s disconnected\n", clients[client_socket].username);

            pthread_mutex_lock(&mutex);
            close(client_socket);
            clients[client_socket].is_active = false;
            pthread_mutex_unlock(&mutex);
            break;
        }
        buffer[bytes_received] = '\0';
        send_message_to_clients(client_socket, buffer);
    }

    pthread_exit(NULL);
}

int main() {
    int server_socket, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t tid;

    // Initialize client array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket = -1; // Mark slots as unused
    }

    // Create a socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Bind the socket to an address and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Binding failed");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == -1) {
        perror("Listening failed");
        exit(1);
    }

    printf("Server is listening for incoming connections...\n");

    while (1) {
        // Accept a client connection
        client_socket = (int *)malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (*client_socket == -1) {
            perror("Acceptance failed");
            continue;
        }

        // Find an available slot in the clients array
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == -1) {
                clients[i].socket = *client_socket;
                break;
            }
        }

        if (i == MAX_CLIENTS) {
            // Reached maximum clients, reject the connection
            close(*client_socket);
            free(client_socket);
            continue;
        }

        // Create a new thread to handle client communication
        pthread_create(&tid, NULL, handle_client, client_socket);
    }

    // Close the server socket
    close(server_socket);
    return 0;
}


