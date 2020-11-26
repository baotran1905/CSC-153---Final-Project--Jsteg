#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int hide_chunk(char* path_to_jpeg, char* chunk, size_t chunk_size)
{
	
	//get file size	
	struct stat st;
	if (stat(path_to_jpeg, &st) == -1)
	{
        	fprintf(stderr, "Error - couldn't stat jpeg at %s\n", path_to_jpeg);
		exit(1);
	}
	int len = st.st_size;
    //create a buffer to hold the jpg + the new data

    size_t size_of_result = (len + chunk_size) * sizeof(unsigned char);
 	unsigned char *data_to_write = (unsigned char*) malloc(size_of_result);
		
	//read jpg into buffer 
	FILE* jpg=fopen(path_to_jpeg, "r");

	
	char tmpBuffer[512];
    int j = 0;
    int end_marker = -1;
    int done = 0;
    while (fread(tmpBuffer, 1, 512, jpg) && (!done))
    {
        for (int i = 0; i < 512; i++)
        {
            data_to_write[j] = tmpBuffer[i];
            if(data_to_write[j-1] == 0xff && data_to_write[j] == 0xd9) // stop copying when we find the jpeg trailer
            {
                done=1;
                j++;
                break;
            }
            j++; //  j now is the array offset to the next empty byte
        }
    }
    size_of_result=(j+chunk_size) * sizeof(unsigned char);

    fclose(jpg);
    //copy the new data into the data_to_write buffer 
    for(int k = 0; k < chunk_size; k++)
    {
        data_to_write[j++] = chunk[k];
    }
    jpg = fopen(path_to_jpeg, "wb");
    if(!jpg)
    {
        fprintf(stderr, "Unable to open %s for writing", path_to_jpeg);
        exit(2);
    }
    fwrite(data_to_write, 1,size_of_result,jpg); // overwrite file with data in buffer
    fclose(jpg);
    free(data_to_write);
	return 0;	
}

unsigned char* extract_chunk(char* path_to_jpeg, int* size)
{
    
	//get file size	
	struct stat st;
	if (stat(path_to_jpeg, &st) == -1)
	{
        	fprintf(stderr, "Error - couldn't stat jpeg at %s\n", path_to_jpeg);
		exit(1);
	}
	int len = st.st_size;
    //create a buffer to hold the file 

    size_t size_of_result = (len) * sizeof(unsigned char);
 	unsigned char *jpg= (unsigned char*) malloc(size_of_result);
		
	//read jpg into buffer 
	FILE* jpgf=fopen(path_to_jpeg, "r");

	
	char tmpBuffer[512];
    int j = 0;
    int end_of_jpg= 0; // the address of the first byte followiung ffd9
    while (fread(tmpBuffer, 1, 512, jpgf))
    {
        for (int i = 0; i < 512; i++)
        {
            jpg[j] = tmpBuffer[i];
            if(jpg[j-1] == 0xff && jpg[j] == 0xd9) // found the jpeg trailer
            {
                end_of_jpg=j;
            }
            j++; 
        }
    }
    size_t size_of_chunk = size_of_result - (end_of_jpg+1);
 	unsigned char *chunk= (unsigned char*) malloc(size_of_chunk); // make space to copy chunk into
    for(int i=0; i< size_of_chunk; i++)
    {
        chunk[i] = jpg[end_of_jpg+1+i];
    }
    free(jpg);
    fclose(jpgf);
    *size = ((int) size_of_chunk);
    return chunk;
}

int main()
{
    
    char *test_jpg = "/media/Enclave/school/sac_state/Courses/2020/Fall/CSC153/CSC-153---Final-Project/chapel_mountain_sky_alps.jpg";

    char test_chunk[14]= "this is a test";
    hide_chunk(test_jpg, test_chunk, 14);

    int size_of_chunk;
    int *size = &size_of_chunk;
    unsigned char *data= extract_chunk(test_jpg, size);

    int i;
    for(i=0; i< size_of_chunk; i++)
    {
        printf("%c", data[i]);   
    }

}
