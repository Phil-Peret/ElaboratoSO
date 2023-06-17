/************************************
*VR421770
*Filippo Peretti
*15/06/2023
*************************************/
#include <stdio.h>
#include <sys/sem.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <errno.h>

//header file per la gestione dei semafori

void semop_siginterrupt(int, struct sembuf*, int);

