# Forza4 - Sistemi operativi
Implementazione del gioco "Forza4" in modalità Client/Server tra processi.

## Sommario
* [Linee guida [PDF]](Doc/2022-23ElaboratoSystemCall.pdf)
* [Informazioni generali](#informazioni-generali)
* [Scelte progettuali](#scelte-progettuali)

## Informazioni generali
Il progetto prevede l'implementazione del famoso gioco "Forza4" in modalità Client/Server (```F4Client.c``` e ```F4Server.c```) tra processi mediante l'utilizzo di IPC basate su SVr4.

## Scelte progettuali
Per la comunicazione tra Client/Server (oltre alla mappa di gioco) è utilizzata una **message queue**. Quest'ultima viene utilizzata sia per la registrazione dei client alla partita sia per lo scambio di informazioni. In particolare, la procedura di registrazione dei client al server avviene in diversi passaggi:
### Registrazione alla partita
#### CLIENT : Prenotazione semaforo e scrittura del pid nella message queue
Prima di poter accedere alla message queue deve eseguire una prenotazione al semaforo che gestisce l'accesso ai client per la registrazione (valore iniziale 2), in caso di successo il client può scrivere il messaggio inserendo il proprio pid. In questo caso il msgtype è settato a 1. 
#### SERVER : Attesa in message queue di nuovi messaggi
Il server si mette in attesa dei messaggi di registrazione alla partita. Appena un nuovo messaggio viene inserito, lo legge e risponde al client con tutte le informazioni necessarie per poter partecipare alla partita, settando il msgtype uguale al PID del client. In questo modo il server è in grado di distribuire al meglio le risorse assegnado ad ogni client un specifco semaforo.

### Turni di gioco
I turni di gioco vengono gestiti dal server tramite un set di semafori per ogni client, necessari per la corretta sincronizzazione dei turni.



