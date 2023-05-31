#include "semaphore.h"
#include <errno.h>

void semop_siginterrupt(int sem_id, struct sembuf *sops, int n_ops){
	int ret;
	do{
		ret = semop(sem_id, sops, n_ops);
	}while(ret == -1 && errno == EINTR);

	if(ret == -1){
		if(errno == EIDRM)
			perror("Server Il server si è spento");
		else
			perror("Error in semop!");
	}
}