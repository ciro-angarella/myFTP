#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define PORT 50000

int main() {
    int serverSocket, clientSockets[MAX_CLIENTS], maxClients = MAX_CLIENTS;
    fd_set readSet, writeSet;
    char buffer[BUFFER_SIZE];

    // Create a socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 50000...\n");

    // Initialize client sockets array
    for (int i = 0; i < maxClients; ++i) {
        clientSockets[i] = 0;
    }

    while (1) {
        FD_ZERO(&readSet);
        FD_ZERO(&writeSet);
        FD_SET(serverSocket, &readSet);

        int maxSocket = serverSocket;

        // Add client sockets to the set
        for (int i = 0; i < maxClients; ++i) {
            int clientSocket = clientSockets[i];

            if (clientSocket > 0) {
                FD_SET(clientSocket, &readSet);
                FD_SET(clientSocket, &writeSet);
            }

            if (clientSocket > maxSocket) {
                maxSocket = clientSocket;
            }
        }

        // Use select to monitor sockets
        if (select(maxSocket + 1, &readSet, &writeSet, NULL, NULL) == -1) {
            perror("Error in select");
            exit(EXIT_FAILURE);
        }

        // Check for incoming connections
        if (FD_ISSET(serverSocket, &readSet)) {
            int newSocket;
            if ((newSocket = accept(serverSocket, (struct sockaddr*)NULL, NULL)) == -1) {
                perror("Error accepting connection");
                exit(EXIT_FAILURE);
            }

            printf("New connection accepted\n");

            // Add the new connection to the list of clients
            for (int i = 0; i < maxClients; ++i) {
                if (clientSockets[i] == 0) {
                    clientSockets[i] = newSocket;
                    break;
                }
            }
        }

        // Check for data to read or write on client sockets
    for (int i = 0; i < maxClients; ++i) {
    int clientSocket = clientSockets[i];

    if (clientSocket > 0) {     
        if (FD_ISSET(clientSocket, &readSet) && FD_ISSET(clientSocket, &writeSet)) {

            char buffer[BUFFER_SIZE];
            //pulizia buffer
            memset(buffer, 0, sizeof(buffer));
            //lettura del socket
            ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            //aggiunta terminatore stringa
            buffer[bytesRead] = '\0';
            // printf("Contenuto del buffer ricevuto: %s\n", buffer); per il debug


            if (bytesRead < 0) {
                perror("Errore durante la ricezione dei dati");
            } else if (bytesRead == 0) { //se non riceve dati dal client
                // Connessione chiusa dal client
                printf("Client disconnesso\n");
            } else {
                // se la lettura va a buon fine , gestione dei comandi FTP
                printf("Ho ricevuto dal client: %s\n", buffer);

                const char* hello = "hello";
                int cmpResult = strcmp(buffer, hello); //controllo del comando
                //printf("Risultato del confronto: %d\n", cmpResult); per il debug

                if (cmpResult == 0) {
                   // printf("Il comando è: %s\n", buffer); per il debug

                    // Invia "hello world from server" al client
                    const char* response = "hello world from server";
                    send(clientSocket, response, strlen(response), 0);
                    printf("Ho mandato al client: %s\n", response);
                }
            }
        }
    }
}

        
        
    }

    return 0;
}
