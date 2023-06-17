/************************************
*VR421770
*Filippo Peretti
*15/06/2023
*************************************/
#include "message_queue.h"

void send_message(int msg_id, const void *msg, size_t msg_size, int flag){
	int ret;
	do{
		ret = msgsnd (msg_id, msg, msg_size, flag);
	}while(ret==-1 && errno==EINTR);

	if(ret==-1){
		perror("Error in send message");
		exit(-1);
	}
}

void recive_message(int msg_id, void *msg_rec, size_t msg_size, long int type, int flag){
	int ret;
	do{
		ret = msgrcv(msg_id, msg_rec, msg_size, type, flag);
	}while(ret==-1 && errno==EINTR);

	if(ret==-1){
		perror("Error in recive message");
		exit(-1);
	}
}
