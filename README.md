# Forza4 - Sistemi operativi
Implementazione del gioco "Forza4" in modalità Client/Server tra processi.

## Sommario
* [Linee guida [PDF]](Doc/2022-23ElaboratoSystemCall.pdf)
* [Informazioni generali](#informazioni-generali)
* [Scelte progettuali](#scelte-progettuali)
* [Note](#note)
* [Casi Particolari](#casi-particolari)
* [Prerequisiti](#prerequisiti)
* [Tips&Tricks](#tips&triks)

## Informazioni generali
Il progetto prevede l'implementazione del famoso gioco "Forza4" in modalità Client/Server (```F4Client.c``` e ```F4Server.c```) tra processi mediante l'utilizzo di IPC basate su SVr4.

## Scelte progettuali
***Il programma è stato implementato in modo tale che il server sia (per quanto possibile) responsabile di tutte le operazioni per la gestione della partita.***<br><br>
Per la comunicazione tra Client/Server è utilizzata una **message queue**. Quest'ultima viene utilizzata sia per la registrazione dei client alla partita sia per lo scambio di informazioni. La procedura di registrazione dei client al server avviene in diversi passaggi:
### Registrazione alla partita
#### CLIENT : Prenotazione semaforo e scrittura del pid nella message queue <sup><sub>([reference code](https://github.com/Phil-Peret/ElaboratoSO/blob/da4bfc393a1c99d7fff16d4af7d4908eb8de48f8/F4Client.c#L202-L207))</sub></sup>
Prima di poter accedere alla message queue deve eseguire una prenotazione al semaforo, il quale gestisce l'accesso ai client per la registrazione (valore iniziale 2). In caso di successo il client può scrivere il messaggio inserendo il proprio pid. In questo caso il msgtype è settato a 1. 
#### SERVER : Attesa in message queue di nuovi messaggi <sup><sub>([reference code](https://github.com/Phil-Peret/ElaboratoSO/blob/c27a4536276f16b026af7afcb3efffe40d8bff1a/F4Server.c#L239-L272))</sub></sup>
Il server si mette in attesa dei messaggi di registrazione alla partita. Appena un nuovo messaggio da parte di un client viene inserito, lo legge e risponde al mittente con tutte le informazioni necessarie per poter partecipare alla partita. Il messaggio di rispsta ha msgtype uguale al PID del client, in questo modo il server è in grado di distribuire al meglio le risorse assegnado ad ogni client un specifco semaforo.
### Turni di gioco
I turni di gioco vengono gestiti dal server tramite un set di semafori per ogni client, necessari per la corretta sincronizzazione.

### Gestione doppio <kbd>Ctrl</kbd>+<kbd>C</kbd> per terminazione server <sup><sub>([reference code](https://github.com/Phil-Peret/ElaboratoSO/blob/1a1804c8deef69775ce23bc0480be666b0c4aa3f/F4Client.c#L117-L146))</sub></sup>
La gestione del doppio <kbd>Ctrl</kbd>+<kbd>C</kbd> per la corretta terminazione del server è assegnata alla funzione ```signal_term_server``` in risposta al segnale ```SIGINT```. Quest'ultima esegue le seguenti operazioni:
1. Incrementa la variabile globale ```counter_c``` inizialmente settata a 0.  
2. Avvia un alarm con un timeout di 5 secondi 
3. Avvisa il server che alla seconda ricezione del prossimo segnale di interrupt (entro 5 secondi) il programma terminerà. 

Se l'alarm scade, la variabile globale viene riportata a 0, se invece un secondo ```SIGINT``` viene ricevuto, il server avvisa i client e rimuove tutte le IPC per poi terminare l'esecuzione.

### Client che gioca in modo automatico <sup><sub>([reference code](https://github.com/Phil-Peret/ElaboratoSO/blob/474bfe9c03b3bfcf7941827000c28fcc5b04c923/F4ClientAuto.c#L244-L271))</sub></sup>
In caso uno dei client inserisca come argomento aggiuntivo <kbd> * </kbd>, il server deve generare un secondo client in grado di giocare alla partita in modo automatico. Poichè corrisponde anche ad un carattere speciale per esegure dei regex sui nomi dei file (es. s* = semaphore.c  semaphore.h => ```rm s*``` = rimuove i due file), il client controlla se gli argomenti aggiuntivi sono uguali al numero di file persenti nella cartella corrente. <br>
Se questo avviene, il client al momento della registrazione nella message queue, setta il valore di ```vs_cpu``` a 1. Alla lettura del messaggio da parte del server, viene generato un processo figlio che esegue  ```F4ClientAuto.o```, un client che al momento del proprio turno è in grado di eseguire l'inserimento del gettone in modo automatico.

## Note
* Se un client si trova in attesa di un altro gicatore non può terminare la propria esecuzione. Rimane comunque possibile terminare il server. 
* Tutte le ```semop``` sono inserite all'interno di un do while per evitare l'uscita del processo dall'attesa:
```c
do{
  ret = semop(sem_id, sops, n_ops);
}while(ret == -1 && errno == EINTR);
```
* Nella compilazione dei sorgenti con il comando ```make``` sono presenti alcuni warning (SA_RESTART fa parte di POSIX):
```
warning: ‘siginterrupt’ is deprecated: Use sigaction with SA_RESTART instead [-Wdeprecated-declarations]
```
## Casi Particolari
### Gestione dei segnali durante syscall
In SVr4 alcune syscall possono essere interrotte dall'arrivo di segnali, come ad esempio le semop e le msgrcv/msgsnd. Per evitare che queste chiamate vengano interrotte, è necessario effettuare un controllo al termine della loro esecuzione, controllando la variabile errno.
Come citato in precedenza nel capitolo [Note](#note), il controllo viene effettuato nel ciclo do-while in modo tale da ripetere l'esecizione.

### Gestione dei segnali durante l'attesa di input
In questo caso abbiamo il problema opposto: le operazioni di input sono bloccanti e non possono esseere interrotte di default. In caso di arrivo di un segnale, quest'ultime vengono riprese immediatamente dopo l'handler del segnale. In caso sia necessario sospendere questa operazione al momento dell'arrivo di un segnale è possibile utilizzare siginterrupt. In tal modo, i segnali desiderati hanno la capacità di sospendere le syscall e bloccare la loro esecuzione.
```c
#include <signal.h>
int siginterrupt(int sig, int flag);
```
*** N.B Nelle ultime versioni di gcc, siginterrupt è indicato come deprecato ***
## Prerequisiti
È necessario il comando ```make``` per la compilazione del codice
* ### Ubuntu
```
sudo apt-get install make
```
* ### Arch
```
sudo pacman -S make
```

## Tips&Tricks
### Giocare in multiplayer
È possibile giocare in modalità multiplayer utilizzando due computer, uno dei due farà da server/client e l'altro solo da client. Sarà sufficente installare il pacchetto openSSH e configurare le due macchine per la connssione.

* ### Ubuntu
```
sudo apt-get install openssh-server
```
* ### Arch
```
sudo pacman -S openssh-server
```


