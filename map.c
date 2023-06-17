#include "map.h"

void clean_map(char *campo, int height, int width){ //pulizia campo
	for(int i=0; i<(height*width); i++){
		campo[i]= ' ';
	}
}

void insert_getton_on_map(char map[], int width, int height, int pos, char symbol){
	//that's simple?
	for (int i=((height*width)-1)+(pos-(width-1)); i>=0; i=i-width){
		if(map[i]==' '){
			map[i]=symbol;
			break;
		}
	}
}

void print_map(char map[],int width,int height){
	for(int i = 0; i<height; i++){
		if(i < 10) printf("  %i ",i);
		else printf(" %i ",i);
	}
	printf("\n");

	for (int i=0; i<height; i++){
	printf("| ");
		for(int j=0; j<width; j++){
				printf("%c | ",map[(i*width)+j]);
		}
	printf("\n");
	}
}

char get_value_by_position(char* map, int width, int height, int y, int x){
	if((x < width && x>=0) && (y < height && y >= 0)){
		return map[(y*width)+x];
	}
	return 'E';
}

int check_map(char* map, int width, int height, char symbol[]){
	//check parità
	int valid=0;
	for (int i=0; i<width; i++){
		if(map[i] != ' '){
			valid++;
		}
	}
	if (valid==width){
		return -1;
	}
	//check riga
	for (int i=0; i<height; i++){
		for (int j=0,p1=0,p2=0; j<width; j++){
			if(get_value_by_position(map, width, height, i, j) == symbol[0]){
				p1++;
				p2=0;
			}
			else if (get_value_by_position(map, width, height, i, j) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, i, j) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check colonna
	for (int i=0,p1=0,p2=0; i<width; i++){
		for (int j=0; j<height; j++){
			if(get_value_by_position(map, width, height, j, i) == symbol[0]){
				p1++;
				p2=0;
			}
			else if (get_value_by_position(map, width, height, j, i) == symbol[1]){
				p2++;
				p1=0;
			}
			else if (get_value_by_position(map, width, height, j, i) == ' '){
				p2=0;
				p1=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check diagonale sup   |
	for(int i=3; i<height ; i++){
		for(int j=i, k=0, p1=0, p2=0; j>=0 && j<width; j--, k++){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check diagonale inferiore __ sottrazione 3 per limite del campo
	for (int i=1; i<(width-3); i++){
		for(int j=(height-1), k=i, p1=0, p2=0; k<width; k++,j--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if(p1>=4 || p2 >=4){
				return 1;
			}
		}
	}

	//check diagonale superiore destra
	for(int i=height; i>=0; i--){
		for(int j=(height-1), k=(width-1), p1=0, p2=0; j>=0; j--, k--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	//check diagonale inferiore destra
	for(int i=height; i>=2; i--){
		for(int j=i, k=(width-1), p1=0, p2=0; j>=0; j--, k--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}

	for(int i=(width-1); i>=2; i--){
		for(int j=(height-1), k=i, p1=0, p2=0; j>=0; j--, k--){
			if(get_value_by_position(map, width, height, j, k) == symbol[0]){
				p1++;
				p2=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == symbol[1]){
				p2++;
				p1=0;
			}
			else if(get_value_by_position(map, width, height, j, k) == ' '){
				p1=0;
				p2=0;
			}
			if (p1>=4 || p2>=4){
				return 1;
			}
		}
	}
	return 0;
}

