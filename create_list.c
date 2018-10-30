#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define BUFSZ 256

int main(int argc, char *argv[])
{
	struct dirent *dir_entry;
	FILE *outfile = fopen("namelist.txt", "w");
	
	if (outfile == NULL){
	  	fprintf(stderr, "Could not open the file.\n");
	  	return 0;
	}

	DIR *main_dir = opendir(argv[1]);

	if (main_dir == NULL){
	  	fprintf(stderr, "Could not open the main directory.\n");
	  	return 0;
	}
	
	char ch;
	int counter = 0;
	while((dir_entry = readdir(main_dir)) != NULL){
		int i = 0;
		if(dir_entry->d_name[i] == '.') continue;
		while(1){
			if(dir_entry->d_name[i] == '_'){ 
				fputc('\n', outfile);				
				break;
			}
			fputc(dir_entry->d_name[i], outfile);
			i++;
		}
		counter++;
	}

	fprintf(stdout, "%i names were listed.\n", counter);

	fclose(outfile);
	closedir(main_dir);
	
	return 0;
}
