/**
 * @file server.c
 * @author Camilla Cacace (camilla.cacace001@studenti.uniparthenope.it)
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
#include <libgen.h>
#include <sys/sendfile.h>


#define BUFFER_SIZE 1024 /**< Dimensione massima usata dai buffer del server */
#define PORT 50000 /**< Porta dove vengono connessi i client e dove arrivvano i comandi dell'utente */
#define DATA_PORT 50001 /**< Porta dove vengono trasferiti gli output e usata per la trasmissione dei file tra server e client */
#define BACKLOG 4
#define MAX_USER 3 /**< Dimensione della struttura hardcoded che contiene gli user registrati */

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

void serverPI(char* command, int dataSocket, int clientSocket, fd_set command_fds,int fd_clients_sockets[], int i) ;
int ricercaPerNome(struct USER array[], int lunghezza,  char *arg);
int ricercaPerFd(struct USER array[], int lunghezza,  int fd);
void sendFileList(int dataSocket, char *directoryPath);

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

    //setup dell'indirizzo della socket dei dati DTP
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

    //setup della socket per le connessioni al PI
    struct sockaddr_in serverAddr;  /**< indirizzo della socket delle connessioni al server e PI*/
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

    //per comodità d'uso aggiungiamo il socket del server al fds
    FD_SET(serverSocket, &command_fds); 

    max_client_fd = serverSocket; /**< essendo il primo fd aggiunto, il serverSocket viene impostato come il valore massimo */
    fd_clients_sockets[max_client_fd] = 1; 

    //inizializzazione degli indici a 0 (libero) a parte il l'indice del server che è settato a 1 (impegnato)
    for (int i = 0; i < max_client_fd; ++i) {
        fd_clients_sockets[i] = 0;
    }

    //---------------------------- main loop--------------------------------------
    while (1) {
        //inizializzazione del fds
        FD_ZERO(&command_fds);

        //aggiungiamo il socket del server al fds
        FD_SET(serverSocket, &command_fds);


        //aggiunge gli fd dei client al fd set
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


        if (select(max_client_fd + 1, &command_fds, NULL, NULL, NULL) < 0 ) { /**< controllo dei descrittori pronti */
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

           


            //aggiunge il nuovo fd ad un indice libero in fd_clients_sockets
            for (int i = 0; i < max_client_fd; ++i) {
                //se l'indice è libero
                if (fd_clients_sockets[i] == 0) {
                    //gli viene asegnato l'fd di un client
                    fd_clients_sockets[i] = newSocket;
                    break;
                }
            }
        }


        //ciclo principale per l'esecuzione del servizio 
        for (int i = 0; i < max_client_fd; ++i) {
            int clientSocket = fd_clients_sockets[i]; /**< client a cui viene erogato il servizio */

            //se clientSocket è associato a un fd 
            if (clientSocket > 0) {
                //se l'fd è pronto
                if (FD_ISSET(clientSocket, &command_fds)) {
                   
                    //pulizia buffer di servizio prima dell'utilizzo
                    memset(buffer, 0, sizeof(buffer));

                    //viene letto il comando su clientSocket
                    ssize_t bytesRead = read(clientSocket, buffer, BUFFER_SIZE- 1); 

                    //aggiunta terminatore stringa
                    buffer[bytesRead] = '\0';

                    if (bytesRead < 0) { //error

                        perror("Errore durante la ricezione dei dati");

                    }else{

                        //esecuizione della richiesta

                        char *DPI_response_code; /**< buffer per la ricezione dei codici del */
                        
                        //erogazione del servizio al client in esecuzione
                        serverPI(buffer, dataSocket, clientSocket, command_fds, fd_clients_sockets, i);

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
 * di controllo. Manda il numero di porta di "DATA_PORT" al client a cui viene erogato il servizio, aspetta che
 * il client si connette, elabora il comando e invia l'output al client.
 *
 * @param command       La stringa contenente il comando inviato dal client.
 * @param dataSocket    Il descrittore di file associato alla connessione di dati (DTP).
 * @param clientSocket  Il descrittore di file associato alla connessione di controllo del client.
 * @param command_fds   L'insieme di descrittori di file associati ai comandi dei client.
 * @param fd_clients_sockets Array dei descrittori di file dei client.
 * @param i             L'indice corrente nell'array dei descrittori di file dei client (usato per il quit).
 *
 */
void serverPI(char* command, int dataSocket, int clientSocket, fd_set command_fds,int fd_clients_sockets[], int i) {
    
    char* data_port_value = "50001"; /**< stringa contenente il nuemero di porta del DTP */

    
    int user_index; /**< user_index contiene l'indice dell'user in registered_user, il valore -1 di user_index significa che l'fd che sta eseguendo il server è un utente anonimo */
    
    //restitusce l'indice dove è conservata la struttura dell'utente a cui è associato l'fd, se non la trova restituisce -1
    user_index = ricercaPerFd(registered_user,MAX_USER, clientSocket); 
    
    int is_logged; /**< variabile di stato che assume il valore 1 se l'fd servito è assocciato ad un user (loggato), altrimenti il suo valore sarà 0 (anonimo)*/ 

    //se è un utente anonimo
    if(user_index == -1){
        //il client in servizio è un user anonimo
        is_logged = 0;

    //se è un user loggato, controlla il suo log state
    } else if(registered_user[user_index].log_state == 1){ 
        is_logged = 1;
    }

    
    char command_word[20]; /**< comando inserito dall'utente es : retr, stor */
    char arg[BUFFER_SIZE]; /**< argomento del comando  es: nomefile.text */

   
    
    //la stringa command viene divisa nel comando (commando word) e l'oggetto del comando (arg)
    sscanf(command,"%s" "%s",command_word, arg);
    

    //-------------------------------------CONTROLLO DEI COMANDI----------------------------------------------------------------

    if ((strncmp(command_word, "user", 4) == 0)&&(is_logged == 0)){ // se l'utente non è gia loggato
        

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));
        
        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket\n");
            exit(1);
        }

        int find; /**< assume il valore -1 se il nome inserito dal client (arg) non è presente nella struttura che contiene gli user, altrimenti ritorna il suo indice*/

        //controlla che l'nome (arg) inserito dall'untente esista tra gli utenti registrati
        find = ricercaPerNome(registered_user, MAX_USER, arg);

        if((find > -1) && (registered_user[find].clientSocket == -1)){ //se l'utente è trovato e non ha gia un fd assegnato
            

            //gli viene aasegnato un fd
            registered_user[find].clientSocket = clientSocket;
            
            //invia la risposta al client
            char *user_founded ="utente trovato, usa il cmd pass";
            write(newDataSocket, user_founded , strlen(user_founded));
           

        }else if((find > -1) && (registered_user[find].clientSocket != -1)){ //se l'utente è già loggato

           
            //invia la risposta al client
            char *user_ass ="utente gia loggato\n";
            write(newDataSocket, user_ass , strlen(user_ass));

        }else if(find == -1){ //se l'utente non è stato trovato
           
            //invia la risposta al client
            char *not_found = "user non trovato\n";
            write(newDataSocket, not_found, strlen(not_found));
            
        }

        //chiusura della socket del DTP
        close(newDataSocket);
       
       
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


        //viene confrontata la pass dell'user con arg (pass inviata dall'client) e se il client che richiede il servizio ha lo stesso fd asseggnato da user
        if(((strcmp(registered_user[user_index].pass, arg) == 0) && (registered_user[user_index].clientSocket == clientSocket ))){

            //cast alla struttura
            struct USER *pUser = &registered_user[user_index];
            pUser->log_state = 1;

            //invio della risposta
            char user_logged[BUFFER_SIZE];
            sprintf(user_logged, "Welcome in my FTP\n");

            write(newDataSocket, user_logged , strlen(user_logged));

        }//se l'fd non è lo stesso significa che piu client cercano di accedere allo stesso user
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

        
    } else if ((strncmp(command_word, "retr", 4) == 0)) {// per user anonimi e registrati
        
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        
        char file_path[BUFFER_SIZE]; /**< dichiarazione del path da scaricare */

        //Se l'user non è anonimo
        if (user_index > -1){
            //usa il suo dir personale come path
            snprintf(file_path, sizeof(file_path), "%s/%s", registered_user[user_index].directoryPath, arg);
           
        }else{
            //alterimenti usa la dir anonima come path
            snprintf(file_path, sizeof(file_path), "%s/%s", anonDir, arg);
           
        }


        //apertura del file da scaricare
        int file_fd = open(file_path, O_RDONLY);

        //mettendo il seek alla fine del file si calcola la sua dimensione
        off_t file_size = lseek(file_fd, 0, SEEK_END);
        lseek(file_fd, 0, SEEK_SET);

        //invio del file
        sendfile(newDataSocket, file_fd, NULL, file_size);

        //chiusura del file
        close(file_fd);

        //chiusura data socket
        close(newDataSocket);
         

        
    } else if ((strncmp(command_word, "stor", 4)== 0) &&(is_logged == 1)) { //se l'user è loggato
        
    // Manda il numero di porta al client
    write(clientSocket, data_port_value, strlen(data_port_value));

    // Accept della connessione
    int newDataSocket;
    if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0) {
        perror("Error accepting connection to data socket");
        exit(1);
    }


    //estrai il nome del file dal filepath inserito dal user
    char *nome_file;
    nome_file = basename(arg);

    //costruzione del filepath di destinazione dove conservare il file uploadato dall'user
    char file_path_stor[BUFFER_SIZE];
    memset(file_path_stor, 0, sizeof(file_path_stor));
    
    //usando la dir dell'user e il nome del file 
    snprintf(file_path_stor, sizeof(file_path_stor), "%s/%s", registered_user[user_index].directoryPath, nome_file);
   
    // Crea un nuovo file per salvare il contenuto ricevuto con il filepath costruito 
    int file_fd = open(file_path_stor, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd == -1) {
        perror("Errore apertura file");
    
    }

    //buffer che contiene i byte del file ricevuto
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    size_t total_bytes_received = 0;

    //lettura dei bytes ricevuti e la loro scrittura sul file creato
    while ((bytes_received = read(newDataSocket, file_buffer, sizeof(file_buffer))) > 0) {
        if (write(file_fd, file_buffer, bytes_received) != bytes_received) {
            perror("Errore scrittura file");
            
        }
    total_bytes_received += bytes_received;
    }

    if (bytes_received == -1) {
        perror("Errore lettura socket");
    
    }

    //chiusura del file appena ricevuto
    close(file_fd);
   


    //chiusura dtp socket
    close(newDataSocket);


        
    } else if(strncmp(command_word, "rnfr", 4) == 0 && (is_logged == 1)){ //se l'user è loggato
       

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

            // Copia la stringa nel campo rename_from dell'user
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

       
}else if (strncmp(command_word, "rnto", 4) == 0 && is_logged == 1){ //se l'user è loggato

        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ){
            perror("Error accepting connection to data socket");
            exit(1);
        }

        char path_rnto[BUFFER_SIZE]; /**<Contiene il path del nuovo percorso */
        memset(path_rnto, 0, sizeof(path_rnto));
        //costruzione del nuovo percorso
        snprintf(path_rnto, sizeof(path_rnto), "%s/%s", registered_user[user_index].directoryPath, arg);

        //esecuzione del rename 
        if (rename(registered_user[user_index].rename_from, path_rnto) == 0) {
        // Rinominazione riuscita
        char *rnto_success = "150 File rinominato con successo\n";
        write(newDataSocket, rnto_success, strlen(rnto_success));
        
        } else {
        // Rinominazione fallita
        char *rnto_fail = "550 Errore durante la rinominazione del file\n";
        write(newDataSocket, rnto_fail, strlen(rnto_fail));
        }
    //chiusura della socket
    close(newDataSocket);

}else if ((strncmp(command_word, "list", 4) == 0)) { //sia per user registrati che per anonimi
       
        //manda il numero di porta al client
        write(clientSocket, data_port_value, strlen(data_port_value));

        //accepet della connessione
        int newDataSocket;
        if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
            perror("Error accepting connection to data socket");
            exit(1);
        }

        if(user_index > -1){//se l'user è loggato
            // cast all struttura
            struct USER *currentUser = &registered_user[user_index];

            // Chiama la tua funzione per inviare i file al client
            sendFileList(newDataSocket, currentUser->directoryPath);

        } else if(user_index == -1){ // se l'user è anonimo
            // Chiama la tua funzione per inviare i file al client
            sendFileList(newDataSocket, anonDir);
        }
        
        //chiusura data socket
        close(newDataSocket);


        
    } else if (strncmp(command_word, "quit", 4) == 0) { //sia per user registrati che per anonimi

        //se l'fd asseggnato non è su default (user loggato)
        if(user_index > -1){
            // resetta le variabili dell'user a default
            registered_user[user_index].clientSocket = -1;
            registered_user[user_index].log_state = 0;
        }

        // rimuove l'fd dall'insieme degli fds
        FD_CLR(clientSocket, &command_fds); 
        // la sua posizione in fd_clients viene liberata 
        fd_clients_sockets[i] = 0;

        close(clientSocket);
           
        

    } else if ((strncmp(command_word, "dele", 4) == 0) && (is_logged == 1)) {//per user registrati

    //manda il numero di porta al client
    write(clientSocket, data_port_value, strlen(data_port_value));

    //accept della connessione
    int newDataSocket;
    if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
        perror("Error accepting connection to data socket\n");
        exit(1);
    }


    char filePath[BUFFER_SIZE]; //**< usato per contenere il filepath colleggato all'utente concatentano al nome del file (arg) */
    memset(filePath,0, sizeof(filePath)); //pulizia del del filepath

   
    //unione del path della directory dell'utente e del nome del file da eliminare (arg)
    snprintf(filePath, sizeof(filePath), "%s/%s", registered_user[user_index].directoryPath, arg);
    
    int file_removed; /**< flag che conferma l'eliminazione del file, 0 se eliminazione andata a buon fine altrimenti il suo valore è 1 */

    //rimozione del file 
    file_removed = remove(filePath);

    if (file_removed == 0) {
        // Eliminazione riuscita
        char *dele_success = "150 File eliminato con successo\n";
        write(newDataSocket, dele_success, strlen(dele_success));
       
    } else {
        // Eliminazione fallita
        char *dele_fail = "550 Errore durante l'eliminazione del file\n";
        write(newDataSocket, dele_fail, strlen(dele_fail));
        
    }  

    //chiusura del data socket
    close(newDataSocket);

} else {
    //manda il numero di porta al client
    write(clientSocket, data_port_value, strlen(data_port_value));

    //accepet della connessione
    int newDataSocket;
    if ((newDataSocket = accept(dataSocket, (struct sockaddr*)NULL, NULL)) < 0 ) {
        perror("Error accepting connection to data socket");
        exit(1);
    }

    //invio dell'errore
    char* cmd_not_found="comando non riconosciuto\n";
    write(newDataSocket, cmd_not_found, strlen(cmd_not_found));

    
    //chiusura data socket
    close(newDataSocket);
        
    }

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
