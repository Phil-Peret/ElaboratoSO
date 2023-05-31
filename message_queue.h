#include<sys/msg.h>
#include<stdio.h>
#include<errno.h>
#include<signal.h>
#include<stdlib.h>

void send_message(int, const void *, size_t, int);
void recive_message(int, void *, size_t, long int, int);
