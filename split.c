#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

void split_file(char* encrypted_file_path, char* jpg_directory)
{
    // check encrypted file path and calculate size:
    struct stat st_enc;
    if (stat(encrypted_file_path, &st_enc) == -1)
    {
        fprintf(stderr, "Error when open encrypted file %s",
                encrypted_file_path);
        exit(1);
    }
    
    // calculate size of enc file
    size_t file_size = st_enc.st_size * sizeof(unsigned char);
    
    // this was for debug
    //char* p = (char*)malloc(file_size);
    
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
    
    // calculate size of each chunk:
    size_t chunk_size = ceil(file_size / (double)(file_count));
    
    // chunk array to store the data:
    char *chunk = (char*)malloc(chunk_size);
    
    // array to store full path:
    char full_path[300];
    int done = 0;

    // open encrypted file for read
    FILE* enc = fopen(encrypted_file_path, "r");
    
    rewinddir(dir_path);

    // split, get chunk, and hide chunk in the jpeg:
    while (((entry = readdir(dir_path)) != NULL) && (!done))
    {
        
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            // create new full path
            strcpy(full_path, jpg_directory);
            strcat(full_path, "/");
            strcat(full_path, entry->d_name);
            
            // if there are still byte to read from encryption file:
            if (fread(chunk, 1, chunk_size, enc))
            {  
                hide_chunk(full_path, chunk, chunk_size);
            }
            else
                done = 1;
        }
    }

    closedir(dir_path);
    fclose(enc);
    
    free(chunk);
}

/******************************************************************************
 *                              Main Code                                     *
 ******************************************************************************
 */
int main()
{
    char* enc= "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/test/enc/test.hex";
    char* pic = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/test/picture";
    
    split_file(enc, pic);
}