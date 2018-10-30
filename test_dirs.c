#include <stdio.h>
#include <dirent.h>

int main(void)
{
  struct dirent *dir_entry;

  DIR *main_dir = opendir("./submissions");

  if(main_dir == NULL){
    printf("Could not open the main directory.\n");
    return 0;
  }

  while((dir_entry = readdir(main_dir)) != NULL){
	printf("%s\n", dir_entry->d_name);
  }

    closedir(main_dir);
    return 0;
}
