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
    fd_set all_fd; //set for read and write of select.h lib
    char buffer[BUFFER_SIZE];

    // Create a socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Error creating socket");
        exit(1);
    }

    // Set up server address
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0 ) {
        perror("Error binding socket");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) < 0 ) {
        perror("Error listening for connections");
        exit(1);
    }

    printf("Server listening on port 50000...\n");

    /* Initialize client sockets array,
     can handle up to maxClients */
    for (int i = 0; i < maxClients; ++i) {

        //initilize the all the fd to 0, so no socket are associated to the array
        clientSockets[i] = 0; 
    }

    /* main loop */
    while (1) {
        FD_ZERO(&all_fd);  //initialize the fd set for the read
        FD_SET(serverSocket, &all_fd); //add the serverSocket to the read set

       //
        int maxSocket = serverSocket;

        // Add client sockets to the set
        for (int i = 0; i < maxClients; ++i) {
            //store in clientSocket the value of clientSocket's array
            int clientSocket = clientSockets[i];

            //if clientSocket is associated to the array clientSockets
            if (clientSocket > 0) {
                //add the clientSocket in all_fd and 
                FD_SET(clientSocket, &all_fd); 
            }
            
            //update the max value of clientSocket
            if (clientSocket > maxSocket) {
                maxSocket = clientSocket;
            }
        }

        // Use select to monitor sockets
        if (select(maxSocket + 1, &all_fd, NULL, NULL, NULL) < 0 ) {
            perror("Error in select");
            exit(1);
        }

        // Check for incoming connections
        if (FD_ISSET(serverSocket, &all_fd)) {
            int newSocket;
            if ((newSocket = accept(serverSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
                perror("Error accepting connection");
                exit(1);
            }

            printf("New connection accepted\n");

            // Add the new connection to the list of clients
            for (int i = 0; i < maxClients; ++i) {
                //check if clientSockets position is free
                if (clientSockets[i] == 0) {
                    //associate the finded position to the new client socket
                    clientSockets[i] = newSocket;
                    break;
                }
            }
        }

        // Check for data to read or write on client sockets
        for (int i = 0; i < maxClients; ++i) {
            int clientSocket = clientSockets[i]; //the clientSocket that use the service

            //if clientSocket is occuped
            if (clientSocket > 0) {     
                //if clientSocket is ready for read and write
                if (FD_ISSET(clientSocket, &all_fd)) {

                    //main of the server's code...

                    char buffer[BUFFER_SIZE];
                    //clear of the buffer
                    memset(buffer, 0, sizeof(buffer));
                    //reading of socket
                    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
                    //aggiunta terminatore stringa
                    buffer[bytesRead] = '\0';
                    // printf("Contenuto del buffer ricevuto: %s\n", buffer); per il debug


                    if (bytesRead < 0) { //error
                        perror("Errore durante la ricezione dei dati");
                    } else if (bytesRead == 0) { //if not recv byte from the reading
                        // close the client
                        printf("Client disconnesso\n");
                        
                        // Remove the client socket from the set
                        FD_CLR(clientSocket, &all_fd);

                        // Reset the client socket in the array to 0
                        clientSockets[i] = 0;

                        close(clientSocket);


                    } else {
                        // if the reading is right, handle the comand
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
