#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 50000
#define BUFFER_SIZE 1024

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

        // Invia la stringa al server
        write(clientSocket, buffer, strlen(buffer));

        // Termina il ciclo se l'utente inserisce "exit"
        if (strcmp(buffer, "exit") == 0) {
            break;
        }

        // Ricevi la risposta dal server
        memset(buffer, 0, sizeof(buffer));
        //metodo bloccante, quindi aspetta la risposta dal server
        read(clientSocket, buffer, sizeof(buffer));
        printf("Risposta dal server: %s\n", buffer);
    }

    // Chiudi il socket
    close(clientSocket);

    return 0;
}
