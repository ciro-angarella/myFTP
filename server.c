#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define PORT 50000
#define MAX_CONNECTIONS 5

ssize_t full_write(int fd, const void *buffer, size_t count) {
    size_t bytes_written = 0;
    ssize_t n;

    while (bytes_written < count) {
        n = write(fd, buffer + bytes_written, count - bytes_written);
        if (n <= 0) {
            if (n == -1) {
                // Errore durante la scrittura
                perror("Errore nella scrittura sulla socket");
            }
            break;
        }
        bytes_written += n;
    }

    return bytes_written;
}

void handle_client(int fd_client
) {
    char buffer[1024];
    const char helloWorld[] = "Hello World, from Server";


    //manda una stringa hello world al client
    full_write(fd_client, helloWorld, strlen(helloWorld));
    printf("ho mandato %s\n", helloWorld);

    // Chiudi il socket del client dopo che la connessione è terminata
    close(fd_client);
}

int main() {
    int fd_server, fd_client;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pid_t child_pid;

    // Creazione del socket del server
    if ( (fd_server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Errore nella creazione del socket del server");
        exit(EXIT_FAILURE);
    }

    // Inizializzazione della struttura sockaddr_in
    memset(&server_addr, 0, sizeof(server_addr)); 
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Binding del socket del server
    if (bind (fd_server, (struct sockaddr *)&server_addr, sizeof(server_addr) ) == -1) {
        perror("Errore nel binding del socket del server");
        exit(EXIT_FAILURE);
    }

    // Inizio dell'ascolto delle connessioni in arrivo
    if (listen (fd_server, MAX_CONNECTIONS) == -1) {
        perror("Errore nell'ascolto delle connessioni");
        exit(EXIT_FAILURE);
    }

    printf("Server in ascolto sulla porta %d...\n", PORT);

    // Accettazione e gestione delle connessioni in arrivo
    while (1) {
        // Accettazione della connessione
        if ((fd_client = accept (fd_server, (struct sockaddr *)&client_addr, &client_addr_len)) == -1) {
            perror("Errore nell'accettare la connessione");
            continue;
        }

        printf("Connessione accettata da %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Creazione di un processo figlio per gestire la connessione
        if ((child_pid = fork()) == 0) {
            // Questo è il processo figlio
            close (fd_server);  // Il processo figlio non ha bisogno del socket del server

            // Gestisci la connessione del client
            handle_client(fd_client);

            // Esci dal processo figlio dopo aver gestito la connessione
            exit(EXIT_SUCCESS);
        } else if (child_pid > 0) {
            // Questo è il processo padre
            close(fd_client);  // Il processo padre non ha bisogno del socket del client
        } else {
            perror("Errore nella creazione di un processo figlio");
            exit(EXIT_FAILURE);
        }
    }

    // Chiudi il socket del server alla fine
    close (fd_server);

    return 0;
}