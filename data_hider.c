#include <stdio.h>
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
	//read jpg in to memory
 	unsigned char *raw = (unsigned char *)malloc(len * sizeof(unsigned char));
		
	FILE* jpg=fopen(path_to_jpeg, "r");
	
	char tmpBuffer[512];
    int j = 0;
    while (fread(tmpBuffer, 1, 512, jpg))
    {
        for (int i = 0; i < 512; i++)
        {
            raw[j] = tmpBuffer[i];
            j++;
        }
    }
    fclose(jpg);
    printf("tmpbuffer: %s \n done", tmpBuffer);

		


	return 0;	
}

int main()
{
	char* test = "abcdefg";
	int result = hide_chunk("/media/Enclave/school/sac_state/Courses/2020/Fall/CSC153/CSC-153---Final-Project/images_from_free-image-com/chapel_mountain_sky_alps.jpg", test,sizeof(test) );


}
