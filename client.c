#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 50000
#define BUFFER_SIZE 2048




/**
 * @brief Funzione principale del programma client.
 *
 * La funzione crea un socket, si connette a un server remoto, e poi avvia un ciclo di comunicazione
 * interattiva con il server. Per ogni iterazione del ciclo, il client invia una stringa al server e
 * riceve la porta sulla quale deve collegarsi per il data transfer. Successivamente, crea un data socket
 * e si connette al server su tale porta per ricevere dati.
 */
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
        printf("Inserisci una stringa da inviare al server (exit per terminare):\n> ");
        char command_buffer[BUFFER_SIZE];
        fgets(command_buffer, BUFFER_SIZE, stdin);

        // Rimuovi il newline inserito da fgets
        command_buffer[strcspn(command_buffer, "\n")] = '\0';

        //invio comandi al server
       write(clientSocket, command_buffer, strlen(command_buffer));
        //printf("ho mandato: %s", command_buffer);

        if(strcmp(command_buffer,"quit")==0){
            close(clientSocket);
            exit(1);
        }

        memset(command_buffer, 0, sizeof(command_buffer));

        //riceve la porta sulla quale collegarsi per il data transfer
        char port_responsed[BUFFER_SIZE];
        //printf("aspetto la porta DTP\n");
        read(clientSocket, port_responsed, BUFFER_SIZE-1);


        if (strlen(port_responsed) != 0){
            // Stampa la porta data dal server
            printf("la porta del data transfer è :%s\n", port_responsed);

            //setup data port
            int data_port = atoi(port_responsed);
        
            //creazione data socket
            dataSocket = socket(AF_INET, SOCK_STREAM, 0);

            if (dataSocket < 0) {
                perror("Errore nella creazione del data socket");
                exit(1);
            }

            // Configurazione dell'indirizzo del server per il data trasnfer
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

            //riceve file
            char data_buffer[BUFFER_SIZE];
            read(dataSocket, data_buffer, BUFFER_SIZE-1);
            data_buffer[BUFFER_SIZE] = '\0';

            printf("%s\n", data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            
            //chiusura data socket
            close(dataSocket);
        }

    }

    // Chiudi il socket
    close(clientSocket);

    return 0;
}




