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
    int clientSocket;
    struct sockaddr_in serverAddr;
    char buffer[BUFFER_SIZE];

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
        fgets(buffer, BUFFER_SIZE, stdin);

        // Rimuovi il newline inserito da fgets
        buffer[strcspn(buffer, "\n")] = '\0';

        sendCommand(clientSocket, buffer);
        memset(buffer, 0, sizeof(buffer));

        char code_responsed[BUFFER_SIZE];
        receiveResponse(clientSocket, code_responsed, sizeof(code_responsed)-1);


        // Stampa la risposta del server
        printf("Server Response:%s\n", code_responsed);
        memset(code_responsed, 0, sizeof(code_responsed));


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

