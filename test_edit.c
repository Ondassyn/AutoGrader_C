#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
	FILE *file = fopen("test_file.c", "r");
	FILE *out_file = fopen("test_file_dup.c", "w");
	char *p;
	_Bool newline_flag;
	char tmp[256] = {0x0};
	int odd_even;
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
	return 0;
}
