#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 50000
#define BUFFER_SIZE 1024

void sendCommand(int sockfd,  char *command);
void receiveResponse(int sockfd, char *buffer, size_t buffer_size);

int main() {
    int clientSocket, dataSocket;
    struct sockaddr_in serverAddr, dataAddr;
    
    // Creazione del socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Errore nella creazione del socket");
        exit(1);
    }

    // Configurazione dell'indirizzo del server
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Indirizzo IP del server

    // Connessione al server
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Errore nella connessione al server");
        close(clientSocket);
        exit(1);
    }

    // Ciclo di comunicazione con il server
    while (1) {
        printf("Inserisci una stringa da inviare al server (exit per terminare): ");
        char command_buffer[BUFFER_SIZE];
        fgets(command_buffer, BUFFER_SIZE, stdin);

        // Rimuovi il newline inserito da fgets
        command_buffer[strcspn(command_buffer, "\n")] = '\0';

        sendCommand(clientSocket, command_buffer);
        memset(command_buffer, 0, sizeof(command_buffer));

        char port_responsed[BUFFER_SIZE];
        receiveResponse(clientSocket, port_responsed, sizeof(port_responsed)-1);


        // Stampa la risposta del server
        printf("la porta del data transfer è :%s\n", port_responsed);

        //setup data socket
        int data_port = atoi(port_responsed);
        
        dataSocket = socket(AF_INET, SOCK_STREAM, 0);

        if (dataSocket < 0) {
            perror("Errore nella creazione del data socket");
            exit(1);
        }

        // Configurazione dell'indirizzo del server
        dataAddr.sin_family = AF_INET;
        dataAddr.sin_port = htons(data_port);
        dataAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Indirizzo IP del server

        memset(port_responsed, 0, sizeof(port_responsed));

        // Connessione al server
        if (connect(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
            perror("Errore nella connessione al data port");
            close(dataSocket);
            exit(1);
        }

        char data_buffer[BUFFER_SIZE];
        receiveResponse(dataSocket,data_buffer, sizeof(data_buffer)-1);

        printf("%s\n", data_buffer);
        memset(data_buffer, 0, sizeof(data_buffer));
        
        close(dataSocket);


    }

    // Chiudi il socket
    close(clientSocket);

    return 0;
}

//-------------------CLIENT PI FUNC----------------------



void sendCommand(int sockfd, char *command) {
    // Invia il comando al server
    if ( write(sockfd, command, strlen(command)) < 0) {
        perror("Errore nell'invio del comando\n");
        exit(1);
    }

}

void receiveResponse(int sockfd, char *buffer, size_t buffer_size) {
    memset(buffer, 0, buffer_size);

    // Ricevi la risposta dal server
    ssize_t bytesRead = read(sockfd, buffer, buffer_size - 1);
    if (bytesRead < 0) {
        perror("Errore nella ricezione della risposta\n");
        exit(1);
    }

    // terminatore del buffer
    buffer[bytesRead] = '\0';
}

