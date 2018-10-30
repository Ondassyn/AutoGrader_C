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
#include <dirent.h>
#include <poll.h>

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

FILE *output_file;
FILE *full_log;

_Bool editted;

_Bool error_flag = 0;


void error_end( char *msg );

void write_to_file(FILE *file, char *msg){
	char *p;
	char tmp[256] = {0x0};
	while(file && fgets(tmp, sizeof(tmp), file)){		
		if((p = strstr(tmp, msg)) != NULL){
			return;
		}
	}
	fprintf(file, "%s", msg);
}

void read_from_child( char *b )
{
	int n, i;
	i = 0;
	int counter = 0;

	//fcntl(childout, F_SETFL, O_NONBLOCK);
	struct pollfd pfd[1];
	pfd[0].fd = childout;
	pfd[0].events = POLLIN;
	pfd[0].revents = POLLHUP;
	int p;

	for (;;)
	{
		if (offset == 0 || mainbuf[offset] == 0)
		{
		  	
			p = poll(pfd, 1, 3000);
			if(p == 0){
				error_end("TIMED OUT\n");
				return;
			}
		  	n = read( childout, mainbuf, BUFSZ );
			
			if ( n == 0 ){
				error_end( "Child exited unexpectedly\n" );
				return;
			}
			mainbuf[n] = 0;
			fprintf( stderr, "MAIN*%s*\n", mainbuf );
			fprintf( full_log, "MAIN*%s*\n", mainbuf );
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
	fprintf( full_log, "READ*%s*\n", b );
}


char *read_and_check( char *b, char *s )
{
	char *p;
	char msg[64];
	
	read_from_child( b );
	
	if(error_flag) return p;
	
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
	int last_guess, pre_last_guess = -1;
	char b[BUFSZ];
	char *sp;
	int gmin = 0, gmax = max;
	int i;
	int MAX = 50;
	
	// If you can't win the game, something is wrong
	for ( i = 0; i < MAX; i++ )
	{
		guess = (int)((gmax-gmin)/2) + gmin;
		if(guess == last_guess)
		  guess++;
		last_guess = guess;
		sprintf( b, "%d\n", guess );
		write( childin, b, strlen(b) );
		fprintf( stderr, "GUESSED %s\n", b );
		fprintf( full_log, "GUESSED %s\n", b );
		usleep(500);

		gc++;
		read_from_child( b );
		if(error_flag) return;		

		if ( strcasestr( b, "LOW" ) != NULL )
			gmin = guess;
		else if ( strcasestr( b, "HIGH" ) != NULL )
			gmax = guess;
		else if (  (sp = strcasestr( b, "TOOK" )) != NULL )
		{
			sscanf( sp, "TOOK %i", &tgc );
			if ( tgc != gc ){
				fprintf( stderr, "GUESSES %d should be %d\n", tgc, gc );
				fprintf( full_log, "GUESSES %d should be %d\n", tgc, gc );
			}
			break;
		}
		else {
			error_end( "Did not answer HIGH/LOW/TOOK\n" );
			if(error_flag) return;
		}
	} 
	if ( i == MAX ){
		error_end( "Game never ends - number out of range?\n" );
		if(error_flag) return;
	}

	total += gc;
	games++;
}


void run_child(char *path)
{
	// CHILD - start the program being tested
    
	fprintf( stderr, "I am the CHILD\n" );
	fprintf( full_log, "I am the CHILD\n" );
	
	close( pipein[1] );
	close( pipeout[0] );
	dup2( pipein[0], STDIN_FILENO );
	dup2( pipeout[1], STDOUT_FILENO );
	char path_to_file[BUFSZ];
	snprintf(path_to_file, sizeof(path_to_file), "%sout", path);
	if ( execlp( path_to_file, "out", NULL ) < 0 ) {
		printf( "Exec failed.\n" );
		fprintf(output_file, "Exec failed.\n");
		fprintf(full_log, "Exec failed.\n");		
		//exit(0);
		error_flag = 1;
	}
	//printf("Exitted HERE\n");
	exit(0);
}

void begin_parent()
{
	fprintf( stderr, "I am the PARENT\n" );
	fprintf( full_log, "I am the PARENT\n" );

	close( pipein[0] );
	close( pipeout[1] );
	childin = pipein[1];
	childout = pipeout[0];

	// wait a second ...
	sleep(1);
}

void edit_submission(char *path, char *name)
{
	if(strcmp(name, "out.c") == 0) return;

  	char path_to_new_file[BUFSZ];
  	snprintf(path_to_new_file, sizeof(path_to_new_file), "%sout.c", path);	
	
	char path_to_old_file[BUFSZ];
	snprintf(path_to_old_file, sizeof(path_to_old_file), "%s%s", path, name);
	
	//printf("`\n`\n`%s\n%s\n`\n", path_to_old_file, path_to_new_file);	

	FILE *file = fopen(path_to_old_file, "r");
	FILE *out_file = fopen(path_to_new_file, "w");
	
	if(file == NULL || out_file == NULL){
		fprintf(stderr, "Could not open files for editting newlines\n");
		error_end("Could not open files for editting new lines\n");
		return;
	}

	char *p;
	_Bool newline_flag;
	char tmp[256] = {0x0};
	int odd_even;
	editted = 0;
	while(file && fgets(tmp, sizeof(tmp), file)){
		odd_even = 0;		
		if((p = strstr(tmp, "printf")) != NULL){
			//fputs("printf(\"", out_file);
			newline_flag = 0;
			int size = 0;
			while(tmp[size] != 0){
				size++;
			}
			//printf("\n\n%s\n\n", tmp);
			fseek(file, (0 - size), SEEK_CUR);
			char ch;
			while((ch = fgetc(file)) != '"'){
				fputc(ch, out_file);
			}
			odd_even++;
			fputc(ch, out_file);
			ch = fgetc(file);
			while(ch != EOF && ch != '\n'){
				if(ch == '"'){
					odd_even++;
					if(odd_even % 2 == 0){
						fseek(file, -4, SEEK_CUR);
						char ch2;
						while((ch2 = fgetc(file)) != '"'){
							if(ch2 == '\\'){
								newline_flag = 1;
							}
						}
						if(newline_flag == 0){
							fputc('\\', out_file);
							fputc('n', out_file);
							editted = 1;
						}
					}
					fputc('"', out_file);
				} else {
					fputc(ch, out_file);
				}
	
				ch = fgetc(file);
			}
			fputc('\n', out_file);	
		} else {
			fputs(tmp, out_file);
		}		
	}

	if(file) fclose(file);
	if(out_file) fclose(out_file);
}

void process_assignment(char *dir_name)
{
  
  	min = 0;
  	max = 9999;

 	int pret;
  	pid_t	pid;
  	struct dirent *dir_entry;
  	char *p;
  	char path[BUFSZ];
  	char path_to_file[BUFSZ];
  	snprintf(path, sizeof(path), "./submissions/%s/", dir_name);
  	snprintf(path_to_file, sizeof(path_to_file), "%serror_log.txt", path);
  
  
 	output_file = fopen(path_to_file, "w");

	if(output_file == NULL){
		fprintf(stderr, "Could not open file '%s'\n", path_to_file);
		error_end("Could not open 'Error log'\n");
		return;
	}

  	DIR *main_dir = opendir(path);

  	if (main_dir == NULL){
    		printf("Could not open '%s' directory.\n", path);
    		exit(0);
  	}

  	char filename[BUFSZ];
  	while((dir_entry = readdir(main_dir)) != NULL){
    		if((p = strcasestr( dir_entry->d_name, ".c" )) != NULL){
    			snprintf(filename, sizeof(filename), "%s", dir_entry->d_name);
  			break;
		}
	}
	printf("%s", filename);
	edit_submission(path, filename);

  	char system_call[BUFSZ];
	snprintf(system_call, sizeof(system_call), "gcc -std=c99 -o '%sout' '%sout.c'", path, path);
	system(system_call);

	fprintf(stderr, "PATH TO FILE: %s\n", path_to_file);
	fprintf(stderr, "FILENAME: %s\n", filename);
	fprintf(full_log, "PATH TO FILE: %s\n", path_to_file);
	fprintf(full_log, "FILENAME: %s\n", filename);
  
	fprintf( stderr, "Starting the test 1: normal play\n" );
	fprintf(full_log, "Starting the test 1: normal play\n");
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
	 	run_child(path);
		if(error_flag) return;
	}
	else
	{
		begin_parent();
		if(error_flag) return;		
		test_normal_games();
		if(error_flag) return;
	}

	fprintf( stderr, "Starting the test 2: unrecognized commands and no games\n" );
	fprintf( full_log, "Starting the test 2: unrecognized commands and no games\n" );
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
	  	run_child(path);
		if(error_flag) return;
	}
	else
	{
		begin_parent();
		if(error_flag) return;
		test_no_games();
		if(error_flag) return;
	}

	fprintf( stderr, "Starting the test 3: new_max\n" );
	fprintf( full_log, "Starting the test 3: new_max\n" );
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
  		run_child(path);
		if(error_flag) return;
	}
	else
	{
		begin_parent();
		if(error_flag) return;
		test_newrange();
		if(error_flag) return;
	}

	fclose(output_file);
	closedir(main_dir);
}


int main()
{
  	char *p, *p2;
	int successful_tests = 0;
	int total_tests = 0;

	struct dirent *dir_entry;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	full_log = fopen("./submissions/full_log.txt", "w");

	FILE *success_testers_list;
	success_testers_list = fopen("./submissions/list_of_successful_testers.txt", "w");	
	
	if(success_testers_list == NULL){
		fprintf(stderr, "Could not open file 'List of successful testers'\n");
		error_end("Could not open 'List of successful testers'\n");
		return 0;
	}

	fprintf(success_testers_list, "SUCCESSFUL TESTERS: \n\n");
	//write_to_file(success_testers_list, "SUCCESSFUL TESTERS: \n\n");
	fflush(success_testers_list);

	DIR *main_dir = opendir("./submissions");

	if (main_dir == NULL){
	  printf("Could not open the main directory.\n");
	  return 0;
	}

	while((dir_entry = readdir(main_dir)) != NULL){
	  	if((p = strcasestr(dir_entry->d_name, "..")) == NULL && (p2 = strcasestr(dir_entry->d_name, ".")) == NULL){	  
	    		fprintf( stderr, "\n\n============%s============\n\n", dir_entry->d_name);
			fprintf(full_log, "\n\n============%s============\n\n", dir_entry->d_name);
			total_tests++;
	    		error_flag = 0;
	    		process_assignment(dir_entry->d_name);
	    		
			if(error_flag == 0){
				char name[BUFSZ];
				int i = 0;
				while(dir_entry->d_name[i] != '_'){
					name[i] = dir_entry->d_name[i];
					i++;
				}
				name[i] = 0;

				successful_tests++;
				//fprintf(success_testers_list, "%d)  %s\n", successful_tests, name);	
				char msg[BUFSZ];
				if(editted == 1){				
					snprintf(msg, sizeof(msg), "%d)  %s (without new lines)\n", successful_tests, name);
				} else {
					snprintf(msg, sizeof(msg), "%d)  %s\n", successful_tests, name);
				}
				
				fprintf(success_testers_list, "%s", msg);
				//write_to_file(success_testers_list, msg);
				
				char system_call[BUFSZ];
				snprintf(system_call, sizeof(system_call), "rm -r './submissions/%s'", dir_entry->d_name);
				system(system_call);
				
			} else {
				char system_call[BUFSZ];
				snprintf(system_call, sizeof(system_call), "rm './submissions/%s/out'*", dir_entry->d_name);
				system(system_call);
			}
	  }
	}

	//fprintf(success_testers_list, "\n\nTotal: %d of %d tests were successful\n", successful_tests, total_tests);
	char msg[BUFSZ];
	snprintf(msg, sizeof(msg), "\n\nTotal: %d of %d tests were successful\n", successful_tests, total_tests);
	//write_to_file(success_testers_list, msg);
	fprintf(success_testers_list, "%s", msg);	

	fclose(success_testers_list);
	closedir(main_dir);
	
}


void error_end( char *msg )
{
	fprintf( stderr, "%s\n", msg );
	fprintf( output_file, "%s\n", msg );
	//char to_write[BUFSZ];
	//snprintf(to_write, sizeof(to_write), "%s\n", msg);
	//write_to_file( output_file, to_write);
	fprintf( full_log, "%s\n", msg );
       //exit(-1);
	error_flag = 1;
}

void check_average( float mavg )
{
	char buf[BUFSZ];
	char *sp;
	float avg;

	// TESTER Expecting the MENU message
	read_and_check( buf, "MENU" );
	if(error_flag) return;

	sprintf( buf, "q\n" );
	write( childin, buf, strlen(buf) );
	sp = read_and_check( buf, "AVERAGE" );
	if(error_flag) return;

	sscanf( sp, "AVERAGE %f", &avg );

	char error_msg[BUFSZ];
	if ( abs(avg - mavg) > 1.0){
		fprintf( stderr, "AVERAGE %f should be %f\n", avg, mavg );
		//fprintf( full_log, "AVERAGE %f should be %f\n", avg, mavg );

		snprintf(error_msg, sizeof(error_msg), "AVERAGE %f should be %f\n", avg, mavg);
		error_end(error_msg);
	}
	else {
		fprintf(stderr, "SUCCESSFUL test\n" );
		fprintf(full_log, "SUCCESSFUL test\n" );
	}
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
		
		if(error_flag) return;
		
		// TESTER Expecting the game
		sprintf( buf, "s\n" );
						
		write( childin, buf, strlen(buf) );
				
		sp = read_and_check( buf, "BETWEEN" );
		if(error_flag) return;
		
		sscanf( sp, "BETWEEN %d AND %d", &tmin, &tmax );
				
		if ( tmin != min || tmax != max ){ 
			fprintf( stderr, "MIN %d should be %d, MAX %d should be %d\n", 
						tmin, min, tmax, max );
			fprintf( output_file, "MIN %d should be %d, MAX %d should be %d\n", 
						tmin, min, tmax, max );
			fprintf( full_log, "MIN %d should be %d, MAX %d should be %d\n", 
						tmin, min, tmax, max );
		}

		// TESTER Guess using the correct limits
		// TESTER Get into the game loop
		play_game();
		if(error_flag) return;
	}
}

void test_normal_games()
{
	char	buf[BUFSZ];

	float mavg;
	char *sp;

	fprintf(stderr, "TESTER starting the test.\n" );
	fprintf(full_log, "TESTER starting the test.\n" );
	fflush( stderr );
	// TESTER Expecting the WELCOME message
	read_and_check( buf, "WELCOME" );
	
	if(error_flag) return;
	play_games( 3 );
	//printf("Returned in TEST_NORMAL_GAMES()\n");
	if(error_flag) return;
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
	fprintf(full_log, "TESTER starting the test.\n" );
	fflush( stderr );
	// TESTER Expecting the WELCOME message
	read_and_check( buf, "WELCOME" );
	if(error_flag) return;
	// TESTER Expecting the MENU message
	read_and_check( buf, "MENU" );
	if(error_flag) return;
	// TESTER  Try a new maximum
	sprintf( buf, "n\n" );
	write( childin, buf, strlen(buf) );
	sp = read_and_check( buf, "MAXIMUM" );
	if(error_flag) return;	
	max = 9999999;
	sprintf( buf, "%d\n", max );
	write( childin, buf, strlen(buf) );

	// TESTER now play it
	play_games( 4 );
	if(error_flag) return;
	// TESTER Expecting the MENU message
	read_and_check( buf, "MENU" );
	if(error_flag) return;
	// TESTER  Try a new maximum
	sprintf( buf, "n\n" );
	write( childin, buf, strlen(buf) );
	sp = read_and_check( buf, "MAXIMUM" );
	if(error_flag) return;
	max = 9;
	sprintf( buf, "%d\n", max );
	write( childin, buf, strlen(buf) );

	// TESTER now play it
	play_games( 2 );
	if(error_flag) return;	

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
	fprintf(full_log, "TESTER starting the test.\n" );
        // TESTER Expecting the WELCOME message
        read_and_check( buf, "WELCOME" );
	if(error_flag) return;
	
        // TESTER Expecting the MENU message
        read_and_check( buf, "MENU" );
	if(error_flag) return;

        // TESTER Expecting the UNRECOGNIZED message
        sprintf( buf, "z\n" );
        write( childin, buf, strlen(buf) );
        read_and_check( buf, "UNRECOGNIZED" );
	if(error_flag) return;

        // TESTER Expecting the MENU message
        read_and_check( buf, "MENU" );
	if(error_flag) return;

        // TESTER Expecting the UNRECOGNIZED message
        sprintf( buf, "r\n" );
        write( childin, buf, strlen(buf) );
        read_and_check( buf, "UNRECOGNIZED" );
	if(error_flag) return;

        // TESTER Expecting the AVERAGE message
	avg = 0.0;
	check_average( avg );

}
