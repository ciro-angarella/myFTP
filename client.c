#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 50000
#define MAX_BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    // Creazione del socket del client
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Errore nella creazione del socket del client");
        exit(EXIT_FAILURE);
    }

    // Inizializzazione della struttura sockaddr_in del server
    //memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Indirizzo IP del server
    server_addr.sin_port = htons(PORT);

    // Connessione al server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Errore nella connessione al server");
        exit(EXIT_FAILURE);
    }

    // Ricevi la risposta dal server
    char buffer[MAX_BUFFER_SIZE];
    int bytes_received = read(client_socket, buffer, sizeof(buffer)-1);
    if (bytes_received == -1) {
        perror("Errore nella lettura dalla socket del client");
        exit(EXIT_FAILURE);
    }

    buffer[bytes_received] = '\0'; // Aggiungi il terminatore di stringa
    printf("Risposta dal server: %s\n", buffer);

    // Chiudi il socket del client
    close(client_socket);

    return 0;
}
