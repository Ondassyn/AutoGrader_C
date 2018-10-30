#include<stdio.h>
#include<stdlib.h>
#include<time.h>
int main(){
	setvbuf(stdout, NULL, _IONBF, 0);
	char ff;

	int mys,x,b,c; b=9999;
	double a,y,d;
	printf("WELCOME to the guessing game!\n");
	printf("MENU: (s) to start a game, (n) to set a new range, or (q) to quit:\n");
	a=0; y=0;
	while(ff!='q'){c=0;

	scanf("%c",&ff);
	if(ff=='n'){
			printf("Enter a new Maximum \n");
			scanf("%i",&b);
			printf("MENU: (s) to start a game, (n) to set a new range, or (q) to quit:\n");
		}
	if(ff=='s'){srand(time(NULL));
	mys=rand()%(b+1);
		printf("The secret number is BETWEEN 0 AND %i. Guess:\n",b);
	for(;x!=mys;c++){
		scanf("%i",&x);
		if(x>mys){
			printf("Too HIGH! Guess:\n");
		}
		if(x<mys){
			printf("Too Low! Guess:\n"); printf("Something crazy asdfasdfasdf\n");
		}
		}
	if(x==mys){
		printf("Correct: You TOOK %i guesses!\n",c);  y++;
		printf("MENU: (s) to start a game, (n) to set a new range, or (q) to quit:\n");
	} }
	a=a+c;
	}d=a/y;
	printf("Thanks for playing. Your guess count AVERAGE is %lf\n",d);

	return 0;
}
