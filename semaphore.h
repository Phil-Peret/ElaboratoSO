#include <stdio.h>
#include <sys/sem.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <errno.h>

//header file per la gestione dei semafori

void semop_siginterrupt(int, struct sembuf*, int);

