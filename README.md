---------------------------------------AUTORI----------------------------------------------

1.Cacace Camilla MATR: 0124002645 github: https://github.com/Oneheroday

2.Ciro Angarella MATR: 0124002559 github: https://github.com/ciro-angarella

3.Vincenzo Terracciano MATR:0124001862 github: https://github.com/Enzo0101

codice gruppo: btlu25c203

-------------------------------------------------------------------------------------------
file particolari presenti in questa dir:

1."help.txt"
    contiene una tabella riassuntiva dei comandi, usata dal client con il comando help
    per visuallizare il file nel terminale.

2."file5.txt", "uploadami.txt", "provami.txt"
    file usati per testare l'upload dei file.

3."FTPpath"
    questa dir contiene le dir degli utenti anonimi e registrati. Ogni user registrato ha
    una propria dir identificata dal proprio nome. Tutti gli user anonimi hanno una sola dir
    chiamata "anon".

4."myFTP.pdf"
    documentazione del progetto.

------------------------------------SETUP DEL PROGRAMMA-----------------------------------------------
1. avviare l'eseguibile "servert"
2. avviare l'eseguibile "clientt"
3.(opzionale) in caso di errore di esecuzione dei comandi verificare se la posizione dei path
    delle dir degli user(presenti in server.c ln :71-78 ) corrisponde alla loro posizione reale.
    In caso di chiarimenti vedere la documenatzione.
    Dopo di che ricompilare il file appena modificato con "gcc server.c -o servert"

---------------------------------------ESECUZIONE------------------------------------------------------

avviato il client si sarà connessi automaticamnte al server.Il programma è impostato per un uso locale
sulla stessa macchina. Al momento della connessione il client sarà riconosciuto dal server come anonimo.
Si consiglia al primo avvio di usare il comando help.

-----------------------------ACCEDERE COME USER REGISTRATO---------------------------------------------

per testare il programma come user registrato, loggare come uno dei seguenti user registrati.

NOME: ciro
PASSWORD: marika

NOME: enzo
PASSWORD: insalata

NOME: camilla
PASSWORD: vale


