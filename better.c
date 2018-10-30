#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define BUFSZ 256

char mainbuf[BUFSZ];
int offset;

int min = 0;
int max = 9999;
int total = 0;
int games = 0;

//  Process management globals

int pipein[2];
int pipeout[2];
int childin, childout;

void test_no_games();
void test_newrange();
void test_normal_games();

void error_end( char *msg )
{
	fprintf( stderr, "%s\n", msg );
	exit(-1);
}


void read_from_child( char *b )
{
	int n, i;

	i = 0;
	for (;;)
	{
		if (offset == 0 || mainbuf[offset] == 0)
		{
			n = read( childout, mainbuf, BUFSZ );
			if ( n == 0 )
				error_end( "Child exited unexpectedly\n" );
			mainbuf[n] = 0;
			fprintf( stderr, "MAIN*%s*\n", mainbuf );
			offset = 0;
		}
		for ( ; mainbuf[offset] != '\n' && mainbuf[offset] != 0; i++, offset++ )
			b[i] = mainbuf[offset];
		if (mainbuf[offset] == '\n')
		{
			offset++;
			b[i] = 0;
			break;
		}
		else
			offset = 0;
	}
	fprintf( stderr, "READ*%s*\n", b );
}


char *read_and_check( char *b, char *s )
{
	char *p;
	char msg[64];

	read_from_child( b );
	if ( (p = strcasestr( b, s )) == NULL )
	{
		sprintf( msg, "Bad %s message.", s );
		error_end( msg );
	}
	return p;
}


void play_game()
{
	int tgc, gc = 0;
	int guess;
	char b[BUFSZ];
	char *sp;
	int gmin = 0, gmax = max;
	int i;
	int MAX = 50;
	
	// If you can't win the game, something is wrong
	for ( i = 0; i < MAX; i++ )
	{
		guess = (int)((gmax+1-gmin)/2) + gmin;
		sprintf( b, "%d\n", guess );
		write( childin, b, strlen(b) );
		fprintf( stderr, "GUESSED %s\n", b );
		usleep(500);

		gc++;
		read_from_child( b );

		if ( strcasestr( b, "LOW" ) != NULL )
			gmin = guess;
		else if ( strcasestr( b, "HIGH" ) != NULL )
			gmax = guess;
		else if (  (sp = strcasestr( b, "TOOK" )) != NULL )
		{
			sscanf( sp, "TOOK %i", &tgc );
			if ( tgc != gc )
				fprintf( stderr, "GUESSES %d should be %d\n", tgc, gc );
			break;
		}
		else
			error_end( "Did not answer HIGH/LOW/TOOK\n" );
	} 
	if ( i == MAX )
		error_end( "Game never ends - number out of range?\n" );

	total += gc;
	games++;
}


void run_child()
{
	// CHILD - start the program being tested
	fprintf( stderr, "I am the CHILD\n" );
	
	close( pipein[1] );
	close( pipeout[0] );
	dup2( pipein[0], STDIN_FILENO );
	dup2( pipeout[1], STDOUT_FILENO );
	if ( execlp( "/home/maerizvi/Dropbox/code/hw", "hw", NULL ) < 0 ) {
		printf( "Exec failed.\n" );
		exit(0);
	}
}

void begin_parent()
{
	fprintf( stderr, "I am the PARENT\n" );

	close( pipein[0] );
	close( pipeout[1] );
	childin = pipein[1];
	childout = pipeout[0];

	// wait a second ...
	sleep(1);
}


int main()
{
	int pret;
	pid_t	pid;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);


	fprintf( stderr, "Starting the test 1: normal play\n" );
	total = 0;
	games = 0;
	offset = 0;

	pret = pipe( pipein );
	pret = pipe( pipeout );
	pid = fork();
	if ( pid < 0 )
		printf( "Fork error.\n" );
	else if ( pid == 0 )
	{
		run_child();
	}
	else
	{
		begin_parent();
		test_normal_games();
	}

	fprintf( stderr, "Starting the test 2: unrecognized commands and no games\n" );
	total = 0;
	games = 0;
	offset = 0;

	pret = pipe( pipein );
	pret = pipe( pipeout );
	pid = fork();
	if ( pid < 0 )
		printf( "Fork error.\n" );
	else if ( pid == 0 )
	{
		run_child();
	}
	else
	{
		begin_parent();
		test_no_games();
	}

	fprintf( stderr, "Starting the test 3: new_max\n" );
	total = 0;
	games = 0;
	offset = 0;

	pret = pipe( pipein );
	pret = pipe( pipeout );
	pid = fork();
	if ( pid < 0 )
		printf( "Fork error.\n" );
	else if ( pid == 0 )
	{
		run_child();
	}
	else
	{
		begin_parent();
		test_newrange();
	}
}


void check_average( float mavg )
{
	char buf[BUFSZ];
	char *sp;
	float avg;

	// TESTER Expecting the MENU message
	read_and_check( buf, "MENU" );

	sprintf( buf, "q\n" );
	write( childin, buf, strlen(buf) );
	sp = read_and_check( buf, "AVERAGE" );

	sscanf( sp, "AVERAGE %f", &avg );
	if ( avg != mavg )
		fprintf( stderr, "AVERAGE %f should be %f\n", avg, mavg );
	else
		fprintf(stderr, "SUCCESSFUL test\n" );
}


void play_games( int n )
{
	char buf[BUFSZ];
	int k;
	int tmax, tmin;
	char *sp;

	for ( k = 0; k < n; k++ )
	{
		// TESTER Expecting the MENU message
		read_and_check( buf, "MENU" );

		// TESTER Expecting the game
		sprintf( buf, "s\n" );
		write( childin, buf, strlen(buf) );
		sp = read_and_check( buf, "BETWEEN" );

		sscanf( sp, "BETWEEN %d AND %d", &tmin, &tmax );
		if ( tmin != min || tmax != max ) 
			fprintf( stderr, "MIN %d should be %d, MAX %d should be %d\n", 
						tmin, min, tmax, max );

		// TESTER Guess using the correct limits
		// TESTER Get into the game loop
		play_game();
	}
}

void test_normal_games()
{
	char	buf[BUFSZ];

	float mavg;
	char *sp;

	fprintf(stderr, "TESTER starting the test.\n" );
	fflush( stderr );
	// TESTER Expecting the WELCOME message
	read_and_check( buf, "WELCOME" );

	play_games( 3 );

	// TESTER Expecting the AVERAGE message

	mavg = (float) total/games;
	check_average( mavg );

}


void test_newrange()
{
	char buf[BUFSZ];
	char *sp;
	int k;

	float mavg;
	int tmax, tmin;

	fprintf(stderr, "TESTER starting the test.\n" );
	fflush( stderr );
	// TESTER Expecting the WELCOME message
	read_and_check( buf, "WELCOME" );

	// TESTER Expecting the MENU message
	read_and_check( buf, "MENU" );

	// TESTER  Try a new maximum
	sprintf( buf, "n\n" );
	write( childin, buf, strlen(buf) );
	sp = read_and_check( buf, "MAXIMUM" );
	max = 9999999;
	sprintf( buf, "%d\n", max );
	write( childin, buf, strlen(buf) );

	// TESTER now play it
	play_games( 4 );

	// TESTER Expecting the MENU message
	read_and_check( buf, "MENU" );

	// TESTER  Try a new maximum
	sprintf( buf, "n\n" );
	write( childin, buf, strlen(buf) );
	sp = read_and_check( buf, "MAXIMUM" );
	max = 9;
	sprintf( buf, "%d\n", max );
	write( childin, buf, strlen(buf) );

	// TESTER now play it
	play_games( 2 );

	// TESTER Expecting the AVERAGE message
	mavg = (float) total/games;
	check_average( mavg );

}


void test_no_games()
{
	char buf[BUFSZ];
	char *sp;
	float avg;

	fprintf(stderr, "TESTER starting the test.\n" );
        // TESTER Expecting the WELCOME message
        read_and_check( buf, "WELCOME" );

        // TESTER Expecting the MENU message
        read_and_check( buf, "MENU" );

        // TESTER Expecting the UNRECOGNIZED message
        sprintf( buf, "z\n" );
        write( childin, buf, strlen(buf) );
        read_and_check( buf, "UNRECOGNIZED" );

        // TESTER Expecting the MENU message
        read_and_check( buf, "MENU" );

        // TESTER Expecting the UNRECOGNIZED message
        sprintf( buf, "r\n" );
        write( childin, buf, strlen(buf) );
        read_and_check( buf, "UNRECOGNIZED" );

        // TESTER Expecting the AVERAGE message
	avg = 0.0;
	check_average( avg );

}
