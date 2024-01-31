#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>


#define BUFFER_SIZE 2048
#define PORT 50000
#define DATA_PORT 50001
#define BACKLOG 4
#define MAX_USER 3

/** @section USER STRUCT
 *  @struct USER 
 *  @brief questa è la struttura di un utente nel programma.Gli utenti sono registrati hardcoded 
 *         all'interno del server.
 *          
 * 
 * @param name Il nome dell'utente,può contenere venti caratteri ed  è il parametro che viene 
 *        cercarto con il comando "user".
 * 
 * @param pass Password del profilo,può contenere venti caratteri ed è il parametro  che viene 
 *             cercarto con il comando "pass".
 * 
 * @param log_state Valore booleano che rappresenta lo stato di log dell'utente. Quando l'user 
 *                  si scollega questo flag viene abbassato al suo valore
 *                  di unlogged ( 0 ).
 * 
 * @param clienSocket Al momento della ricerca del nome dell'utente, viene assegnato l'fd  del 
 *                    processo a questa variabile, collegando quindi uno specifico client a 
 *                    uno specifico user. Quando il client si scollega questa risorsa  viene 
 *                    messsa sul suo valore di default ( -1 ).
 * 
 * @param directoryPath stringa che contiene il path della directoty dell'utente.
*/
struct USER{
    char name[20];
    char pass[20];
    int log_state;
    int clientSocket;
    char *directoryPath;
};

/** 
 * @section REGISTERED_USER
 * @brief Questa è la struttura hardcoded all'interno del server, contiene gli utenti registrati
 *        al servizio. Questo server permette anche la connessione e utilizzo del servizio a utenti 
 *        non registrati, utilizzando il servizio in modo anonimo non effettuando l'acessso. Questo
 *        programma non utlizza un'utente fittizzio anonimo. 
*/
struct USER registered_user[MAX_USER] = {
                 {"enzo", "insalata", 0, -1,"/home/angalinux/Desktop/FTPpath/enzo"},
                 {"ciro", "marika", 0, -1, "/home/angalinux/Desktop/FTPpath/ciro"},
                 {"camilla", "FTP", 0, -1, "/home/angalinux/Desktop/FTPpath/camilla"},
                 };


char *anonDir = "/home/angalinux/Desktop/FTPpath/anon";

char* serverPI(char* command, int dataSocket, int clientSocket, fd_set command_fds,int fd_clients_sockets[], int i) ;
int ricercaPerNome(struct USER array[], int lunghezza,  char *arg);
int ricercaPerFd(struct USER array[], int lunghezza,  int fd);
void sendFileList(int dataSocket, int clientSocket, const char *directoryPath);


int main() {
//---------------------------------VARIABILI PER LA GESTIONE DEL MULTIPLEX-----------------------------

    int serverSocket; /**< Socket dove il server ascolta e accetta le connessioni dei client */
    int dataSocket; /**< Socket usato dal DTP del server per eseguire le richieste dei client */

    int fd_clients_sockets[FD_SETSIZE]; /**< contiene gli fd dei client */

    fd_set command_fds; /**< insieme degli fd disponibili a ricevere comandi dai client */
    int max_client_fd; /**< rappresenta il valore più alto tra gli fd **/

    char buffer[BUFFER_SIZE]; /**< buffer di servizio per conservare i comandi ricevuti */

//----------------------------------------DTP PORT-----------------------------------------------------

    
    if ((dataSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) { /**< creazione della socket per il trasferimento dati */
        perror("Error creating data socket");
        exit(1);
    }

    //setup dell'indirizzo della socket dei dati
    struct sockaddr_in dataAddr; /**< inidirzzo della socket dei dati */
    dataAddr.sin_family = AF_INET;
    dataAddr.sin_addr.s_addr = INADDR_ANY;
    dataAddr.sin_port = htons(DATA_PORT);

    
    if (bind(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0 ) { 
        perror("Error binding data socket");
        exit(1);
    }

    
    if (listen(dataSocket, BACKLOG) < 0 ) { 
        perror("Error listening for data  connections");
        exit(1);
    }

//-------------------------------------PI SOCKET----------------------------------------------
    
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        perror("Error creating socket");
        exit(1);
    }

    
    struct sockaddr_in serverAddr;  /**< inidirzzo della socket delle connessioni al server */
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0 ) {
        perror("Error binding socket");
        exit(1);
    }

    
    if (listen(serverSocket, BACKLOG) < 0 ) {
        perror("Error listening for connections");
        exit(1);
    }

    printf("Server listening on port 50000...\n");

//----------------------------------------------------------------------------------------------

    
    FD_SET(serverSocket, &command_fds); /**< per comodità d'uso aggiungiamo il socket del server al fds */

    max_client_fd = serverSocket; /**< essendo il primo fd aggiunto, il serverSocket viene impostato come il valore massimo */
    fd_clients_sockets[max_client_fd] = 1; 


    /**
     * @brief inizializza a 0 gli indici di fd_clients_sockets 
     * 
     * questo ciclo for inizializza come non impegnati a fd gli indici di fd_clients_sockets,
     * escludendo l'indice di serverSocket. 
     * 
     * @details l'indice di serverSocket è max_client_fd fino a quando non vengono connessi client, se client
     *          si connettono questo valore potrebbe cambiare. lo stato non_assegnato/assegnato è 0/1.
     * 
     * @param max_client_fd Il valore massimo possibile per un descrittore di file di un client.
     * @param fd_clients_sockets Array dei descrittori di file dei client.
     */
    for (int i = 0; i < max_client_fd; ++i) {
        fd_clients_sockets[i] = 0;
    }

    //---------------------------- main loop--------------------------------------
    while (1) {
        FD_ZERO(&command_fds); /**< inizializzazione del fds */
        FD_SET(serverSocket, &command_fds); /**< aggiungiamo il socket del server al fds */


        /**
         * @brief Inizializza l'insieme di descrittori di file per i client e aggiorna il valore massimo.
         *
         * Questo ciclo for scorre l'array di descrittori di file dei client (fd_clients_sockets)
         * e per ciascun descrittore non vuoto, lo aggiunge all'insieme di descrittori di file command_fds.
         * Inoltre, tiene traccia del valore massimo tra i descrittori di file dei client attivi.
         *
         * @param max_client_fd Il valore massimo possibile per un descrittore di file di un client.
         * @param fd_clients_sockets Array dei descrittori di file dei client.
         * @param command_fds Insieme di descrittori di file associati ai comandi dei client.
         */
        for (int i = 0; i < max_client_fd; ++i) {
            //assegnazione del descrittore
            int clientSocket = fd_clients_sockets[i];

            //se il client dell'fd asseggnato ha inviato dei comandi
            if (clientSocket > 0) {
                //lo aggiunge al fds 
                FD_SET(clientSocket, &command_fds);
            }

            //se il valore dell'fd è maggiore del valore massimo
            if (clientSocket > max_client_fd) {
                //aggiorna il valore massimo
                max_client_fd = clientSocket;
            }
        }

        /**
         * @brief Utilizzo della funzione select per monitorare i descrittori di file dei socket.
         *
         * Questo blocco di codice utilizza la funzione select per controllare l'attività
         * dei descrittori di file dei socket inclusi nell'insieme command_fds.
         *
         * @param max_client_fd Il valore massimo possibile per un descrittore di file di un client.
         * @param command_fds Insieme di descrittori di file associati ai comandi dei client.
         */
        if (select(max_client_fd + 1, &command_fds, NULL, NULL, NULL) < 0 ) {
            perror("Error in select");
            exit(1);
        }

        // se la socket del server ha una richiesta di connessione
        if (FD_ISSET(serverSocket, &command_fds)) {
            int newSocket; //crea un nuovo fd vuoto
            //il client associa il suo fd a quello appena creato
            if ((newSocket = accept(serverSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
                perror("Error accepting connection");
                exit(1);
            }

            printf("New connection accepted\n");



            /**
             * @brief Aggiunge la nuova connessione alla lista dei client.
             *
             * Questo ciclo for cerca una posizione libera nell'array fd_clients_sockets
             * e associa la nuova connessione (newSocket) a quella posizione.
             * 
             * @details un indice di fd_clients_sockets è considerato libero se il sio valore è 0.
             * se l'indice è occupato da un client il suo valore è maggiore di 0.
             *
             * @param newSocket Il descrittore di file della nuova connessione da aggiungere.
             * @param max_client_fd Il valore massimo possibile per un descrittore di file di un client.
             * @param fd_clients_sockets Array dei descrittori di file dei client.
             */
            for (int i = 0; i < max_client_fd; ++i) {
                //se l'indice è libero
                if (fd_clients_sockets[i] == 0) {
                    //gli viene asegnato l'fd di un client
                    fd_clients_sockets[i] = newSocket;
                    break;
                }
            }
        }

        /**
         * @brief Verifica la presenza di dati da leggere sui socket dei client.
         *
         * Questo ciclo for verifica ogni descrittore di file nei client sockets
         * per determinare se è pronto per la lettura del comando.
         *
         * @param max_client_fd Il valore massimo possibile per un descrittore di file di un client.
         * @param fd_clients_sockets Array dei descrittori di file dei client.
         * @param command_fds Insieme di descrittori di file associati ai comandi dei client.
         * @param dataSocket Il descrittore di file associato ai dati provenienti dai client.
         */
        for (int i = 0; i < max_client_fd; ++i) {
            int clientSocket = fd_clients_sockets[i]; //scelta del client a cui eseguire il servizio

            //se clientSocket è associato a un fd 
            if (clientSocket > 0) {
                //se l'fd è pronto
                if (FD_ISSET(clientSocket, &command_fds)) {
                   
                    //pulizia buffer di servizio prima dell'utilizzo
                    memset(buffer, 0, sizeof(buffer));
                    //lettura della socket
                    ssize_t bytesRead = read(clientSocket, buffer, BUFFER_SIZE- 1); 
                    //aggiunta terminatore stringa
                    buffer[bytesRead] = '\0';

                    printf("Contenuto del buffer ricevuto: %s\n", buffer);


                    if (bytesRead < 0) { //error

                        perror("Errore durante la ricezione dei dati");

                    }else{

                        //esecuizione della richiesta

                        char *DPI_response_code; //buffer per la ricezione dei codici del DTP
                        //printf("elaborazione richiesta\n");
                        DPI_response_code = serverPI(buffer, dataSocket, clientSocket, command_fds, fd_clients_sockets, i);
                        memset(buffer, 0, sizeof(buffer));
                        
                    }

                    
                }
            }
        }

    }

    return 0;
}



//----------------------SERVER PI----------------------
char* serverPI(char* command, int dataSocket, int clientSocket, fd_set command_fds,int fd_clients_sockets[], int i) {
    char* code_str = NULL;  // Inizializzazione a NULL di default
    char* data_port_value = "50001"; /**< stringa contenente il nuemero di porta del DTP */

    //restitusce l'indice dove è conservata la struttura dell'utente, se non la trova restituisce -1
    int user_index = ricercaPerFd(registered_user,MAX_USER, clientSocket);
    
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
    if ((strncmp(command_word, "user", 4) == 0)&&(is_logged == 0)){
        

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));
        printf("porta inviata\n");

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        int find = ricercaPerNome(registered_user, MAX_USER, arg);

        if((find > -1) && (registered_user[find].clientSocket == -1)){ //se l'utente è trovato e non ha gia un fd assegnato
            printf("utente trovato\n");

            registered_user[find].clientSocket = clientSocket;
            //registered_user[find].finded =1;

            char *user_founded ="utente trovato, usa il cmd pass";
            write(newDataSocket, user_founded , strlen(user_founded));
           

        }else if((find > -1) && (registered_user[find].clientSocket != -1)){

        printf("utente gia assegnato\n");

            
            char *user_ass ="utente gia loggato\n";
            write(newDataSocket, user_ass , strlen(user_ass));

        }else if(find == -1){
            printf("utente non trovato\n");
            char *not_found = "user non trovato\n";
            write(newDataSocket, not_found, strlen(not_found));
            

        }

        close(newDataSocket);
       
        code_str = "331";
    } else if ((strncmp(command_word, "pass", 4)== 0 )&& (registered_user[user_index].clientSocket == clientSocket) && (is_logged == 0)) {
        


        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        //int find = ricercaPerFd(registered_user, MAX_USER, clientSocket); non serve ma non toccare
        printf("%s è stato trovato\n" ,registered_user[user_index].name);

        //confronto pass
        if(((strcmp(registered_user[user_index].pass, arg) == 0) && (registered_user[user_index].clientSocket == clientSocket ))){
            printf("%s è entrato\n" ,registered_user[user_index].name);
            struct USER *pUser = &registered_user[user_index];
            pUser->log_state = 1;

            char user_logged[BUFFER_SIZE];
            sprintf(user_logged, "Welcome in my FTP %s\n", registered_user[user_index].name);

            write(newDataSocket, user_logged , strlen(user_logged));
        }
        else if(registered_user[user_index].clientSocket != clientSocket){

            char *user_ass = "user gia loggato\n";

            write(newDataSocket, user_ass , strlen(user_ass));
            close(newDataSocket);

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
    } else if ((strncmp(command_word, "list", 4) == 0)) {
       
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        if(user_index > -1){
        // Trova l'utente corrente
        struct USER *currentUser = &registered_user[user_index];

        // Chiama la tua funzione per inviare i file al client
        sendFileList(newDataSocket, clientSocket, currentUser->directoryPath);

        } else if(user_index == -1){
        // Chiama la tua funzione per inviare i file al client
        sendFileList(newDataSocket, clientSocket, anonDir);
        }
        
        //chiusura data socket
         close(newDataSocket);


        code_str = "150";
    } else if (strncmp(command_word, "quit", 4) == 0) {

        // close the client
        if(user_index > -1){
            registered_user[user_index].clientSocket = -1;
            registered_user[user_index].log_state = 0;
        }

        // Remove the client socket from the set
            FD_CLR(clientSocket, &command_fds); 
            // Reset the client socket in the array to 0
            fd_clients_sockets[i] = 0;

            close(clientSocket);
            printf("Client disconnesso\n");

        code_str = "221";

    } else if ((strncmp(command_word, "dele", 4) == 0) && (is_logged == 1)) {
    // Implementa la logica per il comando DELE

    //manda il numero di porta al client
    write(clientSocket, data_port_value, strlen(data_port_value));

    //accepet della connessione
    int newDataSocket;
    if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
        perror("Error accepting connection to data socket\n");
        exit(1);
    }


    char filePath[2048];
    memset(filePath,0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "%s/%s", registered_user[user_index].directoryPath, arg);

    int result = remove(filePath);
    printf("%s\n",filePath);

    if (result == 0) {
        // Eliminazione riuscita
        char *dele_success = "150 File eliminato con successo\n";
        write(newDataSocket, dele_success, strlen(dele_success));
        code_str = "150";
    } else {
        // Eliminazione fallita
        char *dele_fail = "550 Errore durante l'eliminazione del file\n";
        write(newDataSocket, dele_fail, strlen(dele_fail));
        code_str = "550";
    }  

    close(newDataSocket);

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

int ricercaPerNome(struct USER array[], int lunghezza,  char *arg) {
    for (int i = 0; i < lunghezza; i++) {
        if (strcmp(array[i].name, arg) == 0) {

            return i; // Nome trovato, restituisce l'indice
        }
    }

    return -1; // Nome non trovato
}

int ricercaPerFd(struct USER array[], int lunghezza,  int fd) {
    for (int i = 0; i < lunghezza; i++) {
        if (array[i].clientSocket == fd) {
            //printf("ho trovato %s con find state %d", array[i].name, array[i].finded);
            return i; // Nome trovato, restituisce l'indice
        }
    }

    return -1; // Nome non trovato
}

void sendFileList(int dataSocket, int clientSocket, const char *directoryPath) {
    DIR *dir;
        struct dirent *entry;

        // Apri la directory specifica dell'utente
        dir = opendir(directoryPath);
        if (dir == NULL) {
            perror("Errore nell'apertura della directory");
            return;
        }

        // Buffer per contenere la lista dei file come una singola stringa
        char fileListBuffer[BUFFER_SIZE] = "";  // Adjust the size according to your needs

        // Leggi la lista dei file nella directory
        while ((entry = readdir(dir)) != NULL) {
            // Ignora le voci "." e ".."
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Concatena il nome del file al buffer
                strcat(fileListBuffer, entry->d_name);
                strcat(fileListBuffer, "\n");
            }
        }

        // Invia la lista dei file come una singola stringa
        write(dataSocket, fileListBuffer, strlen(fileListBuffer));

        // Chiudi la directory
        closedir(dir);
}
