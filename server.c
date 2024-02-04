/**
 * @file server.c
 * @author Camilla Cacace (you@domain.com)
 * @author Ciro Angarella (ciro.angarella001@studenti.uniparthenope.it)
 * @author Vincenzo Terracciano (vincenzo.terracciano003@studenti.uniparthenope.it)
 * 
 * @brief  dettagli implementativi server
 * 
 * il server implementato gestisce le richieste degli utenti utilizzando il multiplex attraverso la funzione select(),
 * così da poter monitorare contemporaneamnete più descrittori di diversi client. L'implemetazione del server FTP avviene 
 * attraverso l'uso di due porte, "PORT" dove arrivano le richeiste al servizio dei client e la porta "DATA_PORT" per il trasferimento 
 * dei file e dei dati di output. Dopo che il multiplex verifica il fd del client da servire, legge il la stringa inviata dal client 
 * e la inserisce nella funzione DPI che ne interpetrerà i comandi. Il DPI intepetrato il comando, eseguirà le funzioni del DTP necessarie
 * per stabilere una connesione sulla DATA_PORT e inviare il relativo output.
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
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>


#define BUFFER_SIZE 1024
#define PORT 50000
#define DATA_PORT 50001
#define BACKLOG 4
#define MAX_USER 3

/** 
 * @struct USER
 * @brief Rappresenta la struttura di un utente nel programma.
 * 
 * @details Quando un nuovo client si connette al server, il suo fd non viene assocciato a nessun user ed è
 * considerato quindi come un user anonimo.Quando immette il comando "user" il sup fd viene associato al nome 
 * inserito e quando effettua l'accesso con il comando "pass" lo stato dell'user diventa loggato. Al momento dell'uso
 * del comando "quit" il suo fd e il suo log_state vengono riportati ai valori di default: 0 per log_state e -1 per clientSocket.
 */
struct USER{
    char name[20]; /**< Il nome dell'utente. Può contenere venti caratteri ed è il parametro cercato con il comando "user". */
    char pass[20]; /**< La password del profilo. Può contenere venti caratteri ed è il parametro cercato con il comando "pass". */
    int log_state; /**< Valore booleano che rappresenta lo stato di login dell'utente. */
    int clientSocket; /**< fd del processo associato all'utente. */
    char *directoryPath; /**< Stringa che contiene il percorso della directory dell'utente. */
    char rename_from[BUFFER_SIZE]; /**< variabile usata per salvare il path del file che si vuole rinominare */
};


/**  
 * @brief Questa è la struttura degli user registrati hardcoded all'interno del server.
 * 
 * @details Questa struttura contiene gli utenti registrati
 *          al servizio. Questo server permette anche la connessione e utilizzo del servizio a utenti 
 *          non registrati, utilizzando il servizio in modo anonimo non effettuando l'acessso. Questo
 *          programma non utlizza un'utente fittizzio anonimo. 
*/
struct USER registered_user[MAX_USER] = {
                 {"enzo", "insalata", 0, -1,"/home/angalinux/Desktop/FTPpath/enzo", ""},
                 {"ciro", "marika", 0, -1, "/home/angalinux/Desktop/FTPpath/ciro", ""},
                 {"camilla", "FTP", 0, -1, "/home/angalinux/Desktop/FTPpath/camilla", ""},
                 };


char *anonDir = "/home/angalinux/Desktop/FTPpath/anon"; /**< directory per tutti gli utenti anonimi */

char* serverPI(char* command, int dataSocket, int clientSocket, fd_set command_fds,int fd_clients_sockets[], int i) ;
int ricercaPerNome(struct USER array[], int lunghezza,  char *arg);
int ricercaPerFd(struct USER array[], int lunghezza,  int fd);
void sendFileList(int dataSocket, char *directoryPath);
void receive_file_data(int socket, FILE *file);


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
         * Questo ciclo for verifica ogni descrittore di file in fd_client_sockets
         * per determinare se è pronto per la lettura del comando, se questo è pronto esegue
         * la richiesta del client.
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

                        char *DPI_response_code; /**< buffer per la ricezione dei codici del */
                        //printf("elaborazione richiesta\n");
                        DPI_response_code = serverPI(buffer, dataSocket, clientSocket, command_fds, fd_clients_sockets, i);
                        //pulizia buffer di servizio
                        memset(buffer, 0, sizeof(buffer));
                        
                    }

                    
                }
            }
        }

    }

    return 0;
}



//------------------------------------------------SERVER PI-----------------------------------------------------

/**
 * @brief Gestisce i comandi inviati dal client attraverso la connessione di controllo.
 *
 * Questa funzione interpreta e gestisce i comandi inviati dal client attraverso la connessione
 * di controllo. Esegue operazioni specifiche in base al comando ricevuto, interagendo con il
 * client attraverso la connessione di dati (DTP) quando necessario.
 *
 * @param command       La stringa contenente il comando inviato dal client.
 * @param dataSocket    Il descrittore di file associato alla connessione di dati (DTP).
 * @param clientSocket  Il descrittore di file associato alla connessione di controllo del client.
 * @param command_fds   L'insieme di descrittori di file associati ai comandi dei client.
 * @param fd_clients_sockets Array dei descrittori di file dei client.
 * @param i             L'indice corrente nell'array dei descrittori di file dei client.
 *
 * @return Una stringa contenente il codice di risposta da inviare al client dopo l'esecuzione del comando.
 *         Può essere utilizzato per indicare il successo o il fallimento dell'operazione.
 */
char* serverPI(char* command, int dataSocket, int clientSocket, fd_set command_fds,int fd_clients_sockets[], int i) {
    char* code_str = NULL;  // Inizializzazione a NULL di default
    char* data_port_value = "50001"; /**< stringa contenente il nuemero di porta del DTP */

    
    int user_index; /**< user_index contiene l'indice dell'user in registered_user, il valore -1 di user_index significa che l'fd che sta eseguendo il server è un utente anonimo */
    //restitusce l'indice dove è conservata la struttura dell'utente, se non la trova restituisce -1
    user_index = ricercaPerFd(registered_user,MAX_USER, clientSocket); 
    
    int is_logged; /**< variabile di stato che assume il valore 1 se l'fd servito è assocciato ad un user (loggato), altrimenti il suo valore sarà 0 (anonimo)*/ 

    if(user_index == -1){
        is_logged = 0;

    } else if(registered_user[user_index].log_state == 1){
        is_logged = 1;
    }

    printf("is logged value : %d\n", is_logged);
    char command_word[20]; /**< comando inserito dall'utente es : retr, stor */
    char arg[20]; /**< argomento del comando  es: nomefile.text */

    printf("il comando ricevuto è: %s\n", command);
    
    /**
     * @brief divisione della stringa command
     * 
     * quando l'untente inserisce un comando da terminale es: "dele file.txt", quest'ultimo viene
     * salvato interamente in nella stringa command. La funzione sscanf() divide la stringa command in due 
     * stringhe : "command_word" per i comandi registrati all'interno del DPI e "arg" che contiene l'argomento
     * del comando. Esempio : l'utente invia al server "dele file.txt", quindi command sarà uguale a "dele file.txt",
     * dopo l'esecuzione dell'istruzione sscanf() command assumerà il valore di "dele" e arg "file.txt".
     * 
     * @param command stringa contenente il comando e il suo argomento inviato dal client.
     * @param command_word stringa contenente il comando interpetrabile dal DPI
     * @param arg stringa contenente l'argomento, il soggeto del comando.
     * 
     */
    sscanf(command,"%s" "%s",command_word, arg);
    printf("comando: %s, arg: %s\n", command_word, arg);

    //-------------------------------------CONTROLLO DEI COMANDI----------------------------------------------------------------

    if ((strncmp(command_word, "user", 4) == 0)&&(is_logged == 0)){ // se l'utente non è gia loggato
        

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));
        printf("porta inviata\n");

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        int find; /**< assume come valore l'indice dell'user dell'fd assocciato, il suo valore è -1 se è un utente anonimo*/

        //controlla che l'nome (arg) inserito dall'untente esista tra gli utenti registrati 
        find = ricercaPerNome(registered_user, MAX_USER, arg);

        if((find > -1) && (registered_user[find].clientSocket == -1)){ //se l'utente è trovato e non ha gia un fd assegnato
            printf("utente trovato\n");

            //gli viene aasegnato un fd
            registered_user[find].clientSocket = clientSocket;
            
            //invia la risposta al client
            char *user_founded ="utente trovato, usa il cmd pass";
            write(newDataSocket, user_founded , strlen(user_founded));
           

        }else if((find > -1) && (registered_user[find].clientSocket != -1)){ //se l'utente è già loggato

            printf("utente gia assegnato\n");
             //invia la risposta al client
            char *user_ass ="utente gia loggato\n";
            write(newDataSocket, user_ass , strlen(user_ass));

        }else if(find == -1){ //se l'utente non è stato trovato
            printf("utente non trovato\n");
            //invia la risposta al client
            char *not_found = "user non trovato\n";
            write(newDataSocket, not_found, strlen(not_found));
            
        }

        //chiusura della socket del DTP
        close(newDataSocket);
       
        code_str = "331";
        //se l'fd che sta usando il servizio è stato connesso ad un user e non è loggato 
    } else if ((strncmp(command_word, "pass", 4)== 0 ) && (registered_user[user_index].clientSocket == clientSocket) && (is_logged == 0)) {
        
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        
        printf("%s è stato trovato\n" ,registered_user[user_index].name);

        //viene confrontata la pass dell'user con arg (pass inviata dall'client) e se client che richiede il servizio ha lo stesso fd asseggnato da user
        if(((strcmp(registered_user[user_index].pass, arg) == 0) && (registered_user[user_index].clientSocket == clientSocket ))){

            printf("%s è entrato\n" ,registered_user[user_index].name);
            //cast alla struttura
            struct USER *pUser = &registered_user[user_index];
            pUser->log_state = 1;

            //invio della risposta
            char user_logged[BUFFER_SIZE];
            sprintf(user_logged, "Welcome in my FTP %s\n", registered_user[user_index].name);

            write(newDataSocket, user_logged , strlen(user_logged));

        }//se l'fd non è lo stesso significa che client cercano di accedere allo stesso user
        else if(registered_user[user_index].clientSocket != clientSocket){ 
            
            //invio della risposta
            char *user_ass = "user già loggato";
            write(newDataSocket, user_ass , strlen(user_ass));

            close(newDataSocket);

        }else{ //errore pass sbagliata

            char *worng_pass = "pass errata, riprova\n";
            write(newDataSocket, worng_pass , strlen(worng_pass));

            close(newDataSocket);
        }

        //chiusura del data socket
        close(newDataSocket);

        code_str = "230";
    } else if ((strncmp(command_word, "retr", 4) == 0)) {
        
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        
        char file_path[BUFFER_SIZE];

        //Se l'user non è anonimo
        if (user_index > -1){
            //usa il suo dir personale
            snprintf(file_path, sizeof(file_path), "%s/%s", registered_user[user_index].directoryPath, arg);
            printf("%s", file_path);
        }else{
            //alterimenti usa la dir anonima
            snprintf(file_path, sizeof(file_path), "%s/%s", anonDir, arg);
            printf("%s", file_path);
        }


        int file_fd = open(file_path, O_RDONLY);

        off_t file_size = lseek(file_fd, 0, SEEK_END);
        lseek(file_fd, 0, SEEK_SET);

        //invio del file
        sendfile(newDataSocket, file_fd, NULL, file_size);

        close(file_fd);
        //chiusura data socket
        close(newDataSocket);
         

        code_str = "150";
    } else if ((strncmp(command_word, "stor", 4)== 0) &&(is_logged == 1)) {
        
    // Manda il numero di porta al client
    write(clientSocket, data_port_value, strlen(data_port_value));

    // Accept della connessione
    int newDataSocket;
    if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0) {
        perror("Error accepting connection to data socket");
        exit(1);
    }

    // Estrai il percorso del file dall'argomento del comando
    char file_path[BUFFER_SIZE];
    memset(file_path, 0, sizeof(file_path));
    snprintf(file_path, sizeof(file_path), "%s/%s", registered_user[user_index].directoryPath, arg);
    printf("Ho unito i path: %s\n", file_path);

    // Apri il file con il percorso specificato
    FILE *file_to_receive = fopen(file_path, "w");
    if (file_to_receive != NULL) {
        // Ricevi dati e scrivi nel file
        receive_file_data(newDataSocket, file_to_receive);
        fclose(file_to_receive);

        char *stor_succ = "226 File stored\n";
        write(clientSocket, stor_succ, strlen(stor_succ));
    } else {
        // Errore durante l'apertura del file
        char *stor_fail = "550 Error opening file for storing\n";
        write(clientSocket, stor_fail, strlen(stor_fail));
    }

    close(newDataSocket);
         


        code_str = "150";
    } else if(strncmp(command_word, "rnfr", 4) == 0 && (is_logged == 1)){
       

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 )    
        {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        char rnfr_str[BUFFER_SIZE]; /**< contiene il path completo del file da rinominare */
        //pulizia della stringa rnfr_str
        memset(rnfr_str, 0, sizeof(rnfr_str));

        //unione del path della directory dell'utente e del nome del file da rinominare (arg)
        snprintf(rnfr_str, sizeof(rnfr_str), "%s/%s", registered_user[user_index].directoryPath, arg);
       
        //se il file esiste e la dimensione dei path è corretta
        if ((access(rnfr_str, F_OK) != -1) && (strlen(rnfr_str) < sizeof(registered_user[user_index].rename_from))) {

            // Copia la stringa nel campo rename_from
            strcpy(registered_user[user_index].rename_from, rnfr_str);
            
            //invia il codice di successo al client
            char *rnfr_succ = "150 File found\n";
            write(newDataSocket, rnfr_succ, strlen(rnfr_succ));

        }else {
            // invia codice errore
            char *rnfr_fail = "550 File not found\n";
            write(newDataSocket, rnfr_fail, strlen(rnfr_fail));    
        }
    //chiusura socket
    close(newDataSocket);

       
}else if (strncmp(command_word, "rnto", 4) == 0 && is_logged == 1){

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ){
            perror("Error accepting connection to data socket");
            exit(1);
        }

        char path_da_rn[BUFFER_SIZE];
        memset(path_da_rn, 0, sizeof(path_da_rn));
        snprintf(path_da_rn, sizeof(path_da_rn), "%s/%s", registered_user[user_index].directoryPath, arg);

        if (rename(registered_user[user_index].rename_from, path_da_rn) == 0) {
        // Rinominazione riuscita
        char *rnto_success = "150 File rinominato con successo\n";
        write(newDataSocket, rnto_success, strlen(rnto_success));
        
        } else {
        // Rinominazione fallita
        char *rnto_fail = "550 Errore durante la rinominazione del file\n";
        write(newDataSocket, rnto_fail, strlen(rnto_fail));
        }

    close(newDataSocket);

}else if ((strncmp(command_word, "list", 4) == 0)) {
       
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
        sendFileList(newDataSocket, currentUser->directoryPath);

        } else if(user_index == -1){
        // Chiama la tua funzione per inviare i file al client
        sendFileList(newDataSocket, anonDir);
        }
        
        //chiusura data socket
         close(newDataSocket);


        code_str = "150";
    } else if (strncmp(command_word, "quit", 4) == 0) {

        //se l'fd asseggnato non è su default
        if(user_index > -1){
            // resetta le variabili dell'user a default
            registered_user[user_index].clientSocket = -1;
            registered_user[user_index].log_state = 0;
        }

        // rimuove l'fd dall'insieme degli fds
            FD_CLR(clientSocket, &command_fds); 
            // la sua posizione in fd_clients viene liberata 
            fd_clients_sockets[i] = 0;

            //chiusura del client
            printf("Client disconnesso\n");

            close(clientSocket);
           
        code_str = "221";

    } else if ((strncmp(command_word, "dele", 4) == 0) && (is_logged == 1)) {
    // Implementa la logica per il comando DELE

    //manda il numero di porta al client
    write(clientSocket, data_port_value, strlen(data_port_value));

    //accept della connessione
    int newDataSocket;
    if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
        perror("Error accepting connection to data socket\n");
        exit(1);
    }


    char filePath[2048]; //**< usato per contenere il filepath colleggato all'utente concatentano al nome del file (arg) */
    memset(filePath,0, sizeof(filePath)); //pulizia del del filepath

   
    //unione del path della directory dell'utente e del nome del file da eliminare (arg)
    snprintf(filePath, sizeof(filePath), "%s/%s", registered_user[user_index].directoryPath, arg);
    
    int file_removed; /**< flag che conferma l'eliminazione del file, 0 se eliminazione andata a buon fine altrimenti il suo valore è 1 */

    //rimozione del file 
    file_removed = remove(filePath);

    printf("%s\n",filePath);

    if (file_removed == 0) {
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
       
        
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

         char* cmd_not_found="comando non riconosciuto\n";
         write(newDataSocket, cmd_not_found, strlen(cmd_not_found));

        //chiusura data socket
         close(newDataSocket);
        
        code_str = "500";
    }

    return code_str;
}

/**
 * @brief Ricerca un utente in un array per nome.
 *
 * Questa funzione cerca un utente all'interno di un array di utenti in base al nome
 * tramite una ricerca sequenziale.
 *
 * @param array      L'array di utenti in cui effettuare la ricerca.
 * @param lunghezza  La lunghezza dell'array di utenti.
 * @param arg        Il nome da cercare.
 *
 * @return           Restituisce l'indice dell'utente se trovato, altrimenti restituisce -1.
 */
int ricercaPerNome(struct USER array[], int lunghezza,  char *arg) {
    for (int i = 0; i < lunghezza; i++) {
        if (strcmp(array[i].name, arg) == 0) {

            return i; // Nome trovato, restituisce l'indice
        }
    }

    return -1; // Nome non trovato
}

/**
 * @brief Ricerca un utente in un array per descrittore di file (fd).
 *
 * Questa funzione cerca un utente all'interno di un array di utenti in base al descrittore di file.
 *
 * @param array      L'array di utenti in cui effettuare la ricerca.
 * @param lunghezza  La lunghezza dell'array di utenti.
 * @param fd         Il descrittore di file da cercare.
 *
 * @return           Restituisce l'indice dell'utente se trovato, altrimenti restituisce -1.
 */
int ricercaPerFd(struct USER array[], int lunghezza,  int fd) {
    for (int i = 0; i < lunghezza; i++) {
        if (array[i].clientSocket == fd) {
            //printf("ho trovato %s con find state %d", array[i].name, array[i].finded);
            return i; // Nome trovato, restituisce l'indice
        }
    }

    return -1; // Nome non trovato
}
/**
 * @brief Contiene la definizione della funzione per inviare la lista dei file.
 */

/**
 * @brief Invia la lista dei file presenti in una directory attraverso un socket.
 *
 * Questa funzione apre la directory specificata, legge la lista dei file escludendo "." e "..",
 * e invia la lista come una singola stringa attraverso il socket fornito.
 *
 * @param dataSocket     Il socket per l'invio della lista dei file.
 * @param clientSocket   Il socket del client associato.
 * @param directoryPath  Il percorso della directory da cui ottenere la lista dei file.
 */
void sendFileList(int dataSocket, char *directoryPath) {
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

void receive_file_data(int socket, FILE *file) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;

    // Leggi i dati dal client e scrivili nel file
    while ((bytesRead = read(socket, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytesRead, file);
    }

    if (bytesRead < 0) {
        perror("Errore durante la lettura dei dati dal socket");
    }
}