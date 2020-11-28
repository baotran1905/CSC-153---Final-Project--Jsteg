#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

void reassemble(char* jpg_directory, char* encrypt_reassemble_path)
{   
    // count the number of jpg file in a directory
    int file_count = 0;
    DIR* dir_path;
    
    struct dirent* entry;
    struct stat st;
    
    if (stat(jpg_directory, &st) == -1)
    {
        fprintf(stderr, "Error when open directory %s \n", jpg_directory);
        exit(1);
    }
    
    dir_path = opendir(jpg_directory);
    
    while ((entry = readdir(dir_path)) != NULL)
    {
        if (entry -> d_type == DT_REG)
            file_count++;        
    }
	
	printf("file count: %d\n", file_count);
    // array to store full path:
    char full_path[300];
    int done = 0;
    
	int full_size = 0;
	//int* size = &chunk_size;
	
    rewinddir(dir_path);
	
	/*char *test_jpg = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/test/picture/c.jpg";
	char test_chunk[17]= "This is a test123";
	hide_chunk(test_jpg, test_chunk, 17);
	*/
	/* char* enc2= "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/out.txt";
    char* pic = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/test/picture";
	
	split_file(enc2, pic2);
	*/
	
	int size_of_chunk;
    int *size = &size_of_chunk;
	
	unsigned char* data = NULL;
	unsigned char* file;
	
	
	int i = 0;
	int j = 0;
	while (((entry = readdir(dir_path)) != NULL) && (!done))
    {
		printf("is it here");   
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            // create new full path
            strcpy(full_path, jpg_directory);
            strcat(full_path, "/");
            strcat(full_path, entry->d_name);
            
            // if there are still byte to read from encryption file:
			printf("%s\n", full_path);
		
			data = extract_chunk(full_path, size);
			
			printf("%d", size_of_chunk);
			
			if (full_size == 0)
			{
				file = (unsigned char*)malloc(size_of_chunk*file_count);
			}
			
			if (size_of_chunk != 0)
			{
				for (i = 0; i < size_of_chunk; i++)
				{
					file[j] = data[i];
					j++;
				}
			
				full_size+= size_of_chunk;
			}
			else
				done = 1;
		}
    }
	
	printf("%d", full_size);
	
	FILE* out = fopen(encrypt_reassemble_path, "w");
	fwrite(file, 1, full_size, out);
	fclose(out);
	
	
	free(file);
    closedir(dir_path); 
}

/******************************************************************************
 *                              Main Code                                     *
 ******************************************************************************
 */
int main()
{
    char* enc= "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/out2.txt";
    char* pic = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/test/picture";

    reassemble(pic, enc);
}
