#include<stdio.h>
#include<errno.h>

void clean_map(char *, int, int);
void insert_getton_on_map(char[], int, int, int, char);
char get_value_by_position(char*, int, int, int, int);
int check_winner(char*, int, int, char[]);
void print_map(char*, int, int);
