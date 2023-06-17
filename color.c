/************************************
*VR421770
*Filippo Peretti
*15/06/2023
*************************************/
#include "color.h"

void red(){
	printf("\033[0;31m");
}

void yellow(){
	printf("\033[0;33m");
}

void reset_color(){
	printf("\033[0;37m");
}

void green(){
	printf("\033[0;32m");
}