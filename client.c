/**
 * @file client.c
 * @author Camilla Cacace (you@domain.com)
 * @author Ciro Angarella (ciro.angarella001@studenti.uniparthenope.it)
 * @author Vincenzo Terracciano (vincenzo.terracciano003@studenti.uniparthenope.it)
 * 
 * @brief  dettagli implementativi server hola
 * 
 * 
 * 
 * 
 * @version 0.1
 * @date 2024-02-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define PORT 50000
#define BUFFER_SIZE 2048

int setupAndConnectDataSocket(char* port_responsed);
ssize_t fullread(int fd, void *buffer, size_t n);

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

    int is_logged = 0;
    // Ciclo di comunicazione con il server
    while (1) {
        printf("Inserisci una stringa da inviare al server (exit per terminare):\n> ");
        char command_buffer[BUFFER_SIZE];
        fgets(command_buffer, BUFFER_SIZE, stdin);

        // Rimuovi il newline inserito da fgets
        command_buffer[strcspn(command_buffer, "\n")] = '\0';

        char command_word[10]; /**< comando inserito dall'utente es : retr, stor */
        char arg[100]; /**< argomento del comando  es: nomefile.text */

        sscanf(command_buffer,"%s" "%s",command_word, arg);



        if(strcmp(command_word,"quit")==0){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));
            is_logged = 0;

            close(clientSocket);
            exit(1);

        }else if(strcmp(command_word, "list")==0){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            //riceve la risposta
            char data_buffer[BUFFER_SIZE];
            read(dataSocket, data_buffer, BUFFER_SIZE-1);
            data_buffer[BUFFER_SIZE] = '\0';

            printf("%s\n", data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            
            //chiusura data socket
            close(dataSocket);
            

        }else if(strcmp(command_word, "stor")==0 &&(is_logged == 1)){
            // invio comando, solo la command word
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));
        
            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            int file_fd;
            off_t file_size;
            ssize_t bytes_written;

            // Apertura del file
            file_fd = open(arg, O_RDONLY);
            if (file_fd == -1) {
                perror("Errore apertura file");
                return EXIT_FAILURE;
            }

            // Calcolo della dimensione del file
            file_size = lseek(file_fd, 0, SEEK_END);
            if (file_size == -1) {
                perror("Errore calcolo dimensione file");
                close(file_fd);
                return EXIT_FAILURE;
            }
            lseek(file_fd, 0, SEEK_SET);

            // Invio del file
            bytes_written = sendfile(dataSocket, file_fd, NULL, file_size);
            if (bytes_written == -1) {
                perror("Errore invio file");
                close(file_fd);
                return EXIT_FAILURE;
            }

            close(file_fd);

            printf("\ninviati %zu bytes \n", bytes_written);

            close(dataSocket);



        }else if(strcmp(command_word, "retr")==0){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];

            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            // Crea un nuovo file per salvare il contenuto ricevuto
            int file_fd = open( arg, O_WRONLY | O_CREAT | O_TRUNC, 0666);

            char file_buffer[BUFFER_SIZE];
            ssize_t bytes_received;

            //scrittura sul file con i byte ricevuti
            while ((bytes_received = read(dataSocket, file_buffer, sizeof(file_buffer))) > 0) {
                write(file_fd, file_buffer, bytes_received);
            }
            close(file_fd);

            if (bytes_received == -1) {
                perror("Error receiving file");

            }else{
                printf("250 file ricevuto\n");
            }

     

        memset(file_buffer,0, sizeof(file_buffer));


        close(dataSocket);

        }else if(strcmp(command_word, "rnfr")==0 &&(is_logged == 1)){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            //riceve la risposta
            char data_buffer[BUFFER_SIZE];
            read(dataSocket, data_buffer, BUFFER_SIZE-1);
            data_buffer[BUFFER_SIZE] = '\0';

            printf("%s\n", data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            
            //chiusura data socket
            close(dataSocket);
            
            
        }else if(strcmp(command_word, "rnto")==0 &&(is_logged == 1)){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            //riceve la risposta
            char data_buffer[BUFFER_SIZE];
            read(dataSocket, data_buffer, BUFFER_SIZE-1);
            data_buffer[BUFFER_SIZE] = '\0';

            printf("%s\n", data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            
            //chiusura data socket
            close(dataSocket);
            
            
        }else if(strcmp(command_word, "user")==0 &&(is_logged == 0)){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            //riceve la risposta
            char data_buffer[BUFFER_SIZE];
            read(dataSocket, data_buffer, BUFFER_SIZE-1);
            data_buffer[BUFFER_SIZE] = '\0';

            printf("%s\n", data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            
            //chiusura data socket
            close(dataSocket);
            
            
        }else if(strcmp(command_word, "pass")==0 &&(is_logged == 0)){
            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            //riceve la risposta
            char data_buffer[BUFFER_SIZE];
            read(dataSocket, data_buffer, BUFFER_SIZE-1);
            data_buffer[BUFFER_SIZE] = '\0';

            if(strcmp(data_buffer,"Welcome in my FTP\n")==0){
                is_logged = 1;
            }

            printf("%s\n", data_buffer);
            memset(data_buffer, 0, sizeof(data_buffer));
            
            //chiusura data socket
            close(dataSocket);
            
            
        }else{

            //invio comandi al server
            write(clientSocket, command_buffer, strlen(command_buffer));

            memset(command_buffer, 0, sizeof(command_buffer));

            //riceve la porta sulla quale collegarsi per il data transfer
            char port_responsed[BUFFER_SIZE];
            
            read(clientSocket, port_responsed, BUFFER_SIZE-1);

            dataSocket = setupAndConnectDataSocket(port_responsed);

            //riceve la risposta
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

int setupAndConnectDataSocket(char* port_responsed) {
    // Setup del data port
    int data_port = atoi(port_responsed);

    // Creazione del data socket
    int dataSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (dataSocket < 0) {
        perror("Errore nella creazione del data socket");
        exit(1);
    }

    // Configurazione dell'indirizzo del server per il data trasfer
    struct sockaddr_in dataAddr;
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

    printf("connesso su %d\n", data_port);
    return dataSocket;
}

ssize_t fullread(int fd, void *buffer, size_t n) {
    ssize_t bytesRead = 0;
    ssize_t totalBytesRead = 0;

    while ((bytesRead = read(fd, buffer + totalBytesRead, n - totalBytesRead)) > 0) {
        totalBytesRead += bytesRead;
        if (totalBytesRead == n) {
            // Leggi tutto il necessario
            break;
        }
    }

    if (bytesRead < 0) {
        perror("Errore nella lettura dal socket");
    }

    return totalBytesRead;
}
