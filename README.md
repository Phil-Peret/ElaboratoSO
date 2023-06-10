# Forza4 - Sistemi operativi
Implementazione del gioco "Forza4" in modalità Client/Server tra processi.

## Sommario
* [Linee guida [PDF]](Doc/2022-23ElaboratoSystemCall.pdf)
* [Informazioni generali](#informazioni-generali)
* [Scelte progettuali](#scelte-progettuali)
* [Casi "particolari"](#casi-particolari)

## Informazioni generali
Il progetto prevede l'implementazione del famoso gioco "Forza4" in modalità Client/Server (```F4Client.c``` e ```F4Server.c```) tra processi mediante l'utilizzo di IPC basate su SVr4.

## Scelte progettuali
***Il programma è stato implementato in modo tale che il server sia (per quanto possibile) responsabile di tutte le operazioni per la gestione della partita.***<br><br>
Per la comunicazione tra Client/Server è utilizzata una **message queue**. Quest'ultima viene utilizzata sia per la registrazione dei client alla partita sia per lo scambio di informazioni. In particolare, la procedura di registrazione dei client al server avviene in diversi passaggi:
### Registrazione alla partita
#### CLIENT : Prenotazione semaforo e scrittura del pid nella message queue
Prima di poter accedere alla message queue deve eseguire una prenotazione al semaforo che gestisce l'accesso ai client per la registrazione (valore iniziale 2), in caso di successo il client può scrivere il messaggio inserendo il proprio pid. In questo caso il msgtype è settato a 1. 
#### SERVER : Attesa in message queue di nuovi messaggi
Il server si mette in attesa dei messaggi di registrazione alla partita. Appena un nuovo messaggio viene inserito, lo legge e risponde al client con tutte le informazioni necessarie per poter partecipare alla partita, settando il msgtype uguale al PID del client. In questo modo il server è in grado di distribuire al meglio le risorse assegnado ad ogni client un specifco semaforo.
### Turni di gioco
I turni di gioco vengono gestiti dal server tramite un set di semafori per ogni client, necessari per la corretta sincronizzazione tra i client.

### Gestione doppio <kbd>Ctrl</kbd>+<kbd>C</kbd> per terminazione server
Per la gestione del doppio <kbd>Ctrl</kbd>+<kbd>C</kbd> per la corretta terminazione del server viene chiamata la funzione ```signal_term_server``` in risposta al segnale ```SIGINT```. Questa funizione incrementa la variabile globale ```counter_c``` inizialmente settata a 0, avvia un alarm con un timeout di 5 secondi e avvisa il server che alla seconda ricezione del prossimo segnale di interrupt (entro 5 secondi) il programma terminerà. Se l'alarm scade, la variabile globale viene settata a 0, se invece un secondo ```SIGINT``` viene ricevuto, il server avvisa i client e rimuove tutte le IPC per poi terminare l'esecuzione.

### Client che gioca in modo automatico
In caso uno dei client inserisca come argomento aggiuntivo <kbd> * </kbd> allora il server deve generare un secondo client in grado di giocare alla partita in modo automatico. In questo caso è necessario controllare l'inserimento del carattere al momento dell'esecuzione del client, che però corrisponde anche ad un carattere speciale per esegure dei regex sui nomi dei file (es. s* = semaphore.c  semaphore.h => ```rm s``` = rimuove i due file). Per aggirare il problema, il programma controlla se gli argomenti aggiuntivi sono uguali al numero di file persenti nella cartella corrente.
In tal caso il client avvisa il server al momento della registrazione nella message queue, che crea un processo figlio, mandando in esecuzione ```F4ClientAuto.o```, un client che al momento del proprio turno esegue l'inserimento del gettone in modo automatico.

## Casi "particolari"
Se un client si trova in attesa di un altro gicatore non può uscire dall'attesa, se è necessario terminare il programma, è comunque possibile terminado il server.


