#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>


#define BUFFER_SIZE 1024
#define PORT 50000
#define DATA_PORT 50001
#define BACKLOG 4
#define MAX_USER 3


struct USER{
    char name[20];
    char pass[20];
    int log_state; //0 unlog, 1 logged
    int clientSocket;
    int finded;
};


struct USER registered_user[MAX_USER] = {
                 {"enzo", "insalata", 0, -1, 0},
                 {"ciro", "marika", 0, -1, 0} ,
                 {"camilla", "FTP", 0, -1, 0},
                 };



char* serverPI(char* command, int dataSocket, int clientSocket) ;
ssize_t receiveCommand(int sockfd, char *buffer);
int ricercaNomeUser(struct USER array[], int lunghezza,  char *arg);
int ricercaFdUser(struct USER array[], int lunghezza,  int fd);


int main() {

    int serverSocket; //PI socket
    int dataSocket; //socket for DTP

    int fd_command_sockets[FD_SETSIZE]; //contain the command fd of clients
    int fd_data_sockets;

    fd_set command_fd, data_fd; //set for read and write of select.h lib
    int max_command_sockets, max_data_sockets; //the max fd

    char buffer[BUFFER_SIZE];

//-----------------------DTP PORT--------------------------------------

    // Create the Data socket
    if ((dataSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Error creating data socket");
        exit(1);
    }

    // Set up server address of Data transfer protocol
    struct sockaddr_in dataAddr;
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_addr.s_addr = INADDR_ANY;
    dataAddr.sin_port = htons(DATA_PORT);

    // Bind the PI socket
    if (bind(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0 ) {
        perror("Error binding data socket");
        exit(1);
    }

    // Listen for incoming connections on PI socket
    if (listen(dataSocket, BACKLOG) < 0 ) {
        perror("Error listening for data  connections");
        exit(1);
    }

//----------------------PI SOCKET--------------------------------------
    // Create the PI socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Error creating socket");
        exit(1);
    }

    // Set up server address of PI
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    // Bind the PI socket
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0 ) {
        perror("Error binding socket");
        exit(1);
    }

    // Listen for incoming connections on PI socket
    if (listen(serverSocket, BACKLOG) < 0 ) {
        perror("Error listening for connections");
        exit(1);
    }

    printf("Server listening on port 50000...\n");

    FD_SET(serverSocket, &command_fd);

    max_command_sockets = serverSocket; //the max fd value choose by the select
    fd_command_sockets[max_command_sockets] = 1;


    /* Initialize client sockets array,
     can handle up to maxClients */
    for (int i = 0; i < max_command_sockets; ++i) {

        //initilize the all the fd to 0, so no socket are associated to the array
        fd_command_sockets[i] = 0;
    }

    //---------------------------- main loop--------------------------------------
    while (1) {
        FD_ZERO(&command_fd);  //initialize the fd set for the read and the write
        FD_SET(serverSocket, &command_fd); //add the serverSocket to the read set


        // Add client sockets to the set
        for (int i = 0; i < max_command_sockets; ++i) {
            //store in clientSocket the value of clientSocket's array
            int clientSocket = fd_command_sockets[i];

            //if clientSocket is associated to the array fd_command_sockets
            if (clientSocket > 0) {
                //add the clientSocket in command_fd and
                FD_SET(clientSocket, &command_fd);
            }

            //update the max value of clientSocket
            if (clientSocket > max_command_sockets) {
                max_command_sockets = clientSocket;
            }
        }

        // Use select to monitor sockets
        if (select(max_command_sockets + 1, &command_fd, NULL, NULL, NULL) < 0 ) {
            perror("Error in select");
            exit(1);
        }

        // Check for incoming connections
        if (FD_ISSET(serverSocket, &command_fd)) {
            int newSocket;
            if ((newSocket = accept(serverSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
                perror("Error accepting connection");
                exit(1);
            }

            printf("New connection accepted\n");

            // Add the new connection to the list of clients
            for (int i = 0; i < max_command_sockets; ++i) {
                //check if fd_command_sockets position is free
                if (fd_command_sockets[i] == 0) {
                    //associate the finded position to the new client socket
                    fd_command_sockets[i] = newSocket;
                    break;
                }
            }
        }

        // Check for data to read or write on client sockets
        for (int i = 0; i < max_command_sockets; ++i) {
            int clientSocket = fd_command_sockets[i]; //the clientSocket that use the service

            //if clientSocket is occuped
            if (clientSocket > 0) {
                //if clientSocket is ready for read and write
                if (FD_ISSET(clientSocket, &command_fd)) {

                    //main of the server's code...

                    char buffer[BUFFER_SIZE];
                    //clear of the buffer
                    memset(buffer, 0, sizeof(buffer));
                    //reading of socket
                    ssize_t bytesRead = receiveCommand(clientSocket, buffer);
                    //aggiunta terminatore stringa
                    buffer[bytesRead] = '\0';
                    printf("Contenuto del buffer ricevuto: %s\n", buffer);


                    if (bytesRead < 0) { //error

                        perror("Errore durante la ricezione dei dati");

                    } else if (bytesRead == 0) { //if not recv byte from the reading
                        // close the client
                        printf("Client disconnesso\n");

                        // Remove the client socket from the set
                        FD_CLR(clientSocket, &command_fd);

                        // Reset the client socket in the array to 0
                        fd_command_sockets[i] = 0;

                        close(clientSocket);

                    }else{
                           
                        char *command;
                        //printf("elaborazione richiesta\n");
                        command = serverPI(buffer, dataSocket, clientSocket);
                        memset(buffer, 0, sizeof(buffer));
                        
                            
                        }

                    
                }
            }
        }

    }

    return 0;
}



//----------------------SERVER PI----------------------
char* serverPI(char* command, int dataSocket, int clientSocket) {
    char* code_str = NULL;  // Inizializzazione a NULL di default
    char* data_port_value = "50001";

    //restitusce l'indice dove è conservata la struttura dell'utente, se non la trova restituisce -1
    int user_index = ricercaFdUser(registered_user,MAX_USER, clientSocket);
    
    int is_logged = 0;

    if(user_index == -1){
        is_logged = 0;

    } else if(registered_user[user_index].log_state == 1){
        is_logged = 1;
    }
    printf("is logged value : %d\n", is_logged);
    char command_word[20]; //comando inserito dall'utente es : retr
    char arg[20];// argomento del comando  es: nomefile.text


    printf("il comando ricevuto è: %s\n", command);
    //divide il command dell'utente in due stringhe, command_word e arg;
    sscanf(command,"%s" "%s",command_word, arg);
    printf("comando: %s, arg: %s\n", command_word, arg);



    // Utilizza uno statement switch per gestire i comandi
    if ((strncmp(command_word, "user", 4) == 0)&&(is_logged == 0)) {
        

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        int find = ricercaNomeUser(registered_user, MAX_USER, arg);

        if(find > -1){
            printf("utente trovato\n");

            registered_user[find].clientSocket = clientSocket;
            registered_user[find].finded =1;

            char *user_founded ="utente trovato, usa il cmd pass\n";
            write(newDataSocket, user_founded , strlen(user_founded));
           


        }else if(find == -1){
            printf("utente non trovato\n");
            char *not_found = "user non trovato\n";
            write(newDataSocket, not_found, strlen(not_found));
            

        }

        close(newDataSocket);
       
        code_str = "331";
    } else if ((strncmp(command_word, "pass", 4)== 0 )&& (registered_user[user_index].finded == 1 ) && (is_logged == 0)) {
        
        /*
        
                if(user_index < 0){
            close(clientSocket);
        }
        */


        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        //int find = ricercaFdUser(registered_user, MAX_USER, clientSocket); non serve ma non toccare
        printf("%s è stato trovato\n" ,registered_user[user_index].name);

        //confronto pass
        if((strcmp(registered_user[user_index].pass, arg) == 0 ) && (registered_user[user_index].finded == 1)){
            printf("%s è entrato\n" ,registered_user[user_index].name);
            struct USER *pUser = &registered_user[user_index];
            pUser->log_state = 1;

            char user_logged[BUFFER_SIZE];
            sprintf(user_logged, "Welcome in my FTP %s\n", registered_user[user_index].name);

            write(newDataSocket, user_logged , strlen(user_logged));
        }else{

            char *worng_pass = "pass errata, riprova\n";

            write(newDataSocket, worng_pass , strlen(worng_pass));
            close(newDataSocket);
        }

        close(newDataSocket);

        code_str = "230";
    } else if ((strncmp(command_word, "retr", 4) == 0)) {
        // Implementa la logica per il comando RETR

       
        
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        //trasferimento file...

        /*
        cami qui devi fare l'upload del file
        da server a client
        */

        //chiusura data socket
         close(newDataSocket);
         

        code_str = "150";
    } else if ((strncmp(command_word, "stor", 4)== 0) &&(is_logged == 1)) {
        // Implementa la logica per il comando STOR
         printf("-----SONO LOGGATO------\n"); //test

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        //trasferimento file...

        /*
        cami qui devi fare il download del file
        dal client al server
        */



        //chiusura data socket
         close(newDataSocket);
         


        code_str = "150";
    } else if ((strncmp(command_word, "list", 4) == 0) && (is_logged == 1)) {
        // Implementa la logica per il comando LIST

    

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        //cami implementa qui la list


        //chiusura data socket
         close(newDataSocket);



        code_str = "150";
    } else if (strncmp(command_word, "quit", 4) == 0 && (is_logged == 1)) {
        // Implementa la logica per il comando QUIT
        code_str = "221";
    } else {
        // Comando non riconosciuto
        // Listen for incoming connections on PI socket
        if (listen(dataSocket, BACKLOG) < 0 ) {
            perror("Error listening for data  connections");
            exit(1);
        }
        printf("ho aperto la data port\n");
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }


         char* test="comando non riconosciuto\n";
         write(newDataSocket, test, strlen(test));

        //chiusura data socket
         close(newDataSocket);
         printf("ho chiuso la data port\n");

        code_str = "500";
    }

    return code_str;
}


ssize_t receiveCommand(int sockfd, char buffer[BUFFER_SIZE]) {

    ssize_t bytesRead;
    memset(buffer, 0, BUFFER_SIZE);

    // Ricevi la risposta dal server
    if ((bytesRead = read(sockfd, buffer, BUFFER_SIZE -1 )) < 0) {
        perror("Errore nella ricezione del comando\n");
        exit(1);
    }

    return bytesRead;

}

int ricercaNomeUser(struct USER array[], int lunghezza,  char *arg) {
    for (int i = 0; i < lunghezza; i++) {
        if (strcmp(array[i].name, arg) == 0) {

            return i; // Nome trovato, restituisce l'indice
        }
    }

    return -1; // Nome non trovato
}

int ricercaFdUser(struct USER array[], int lunghezza,  int fd) {
    for (int i = 0; i < lunghezza; i++) {
        if (array[i].clientSocket == fd) {
            //printf("ho trovato %s con find state %d", array[i].name, array[i].finded);
            return i; // Nome trovato, restituisce l'indice
        }
    }

    return -1; // Nome non trovato
}


