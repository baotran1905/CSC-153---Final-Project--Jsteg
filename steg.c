#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <gcrypt.h>
#include <sys/stat.h>

/******************************************************************************
 *                            Ignore - Used functions                                     *
 ******************************************************************************
 */
/*---------------------------Perm ----------------------------------------*/
/**
 * Rotate left the bits of 32-bit value by n-times.
 *
 * @param[in]    uint32_t num: 32- bit number
 *               time: number of time to rotate
 *               
 * @param[out]   32-bit number after rotate left times.
 *
 */
static uint32_t rotl(const uint32_t num, int time)
{
    return (num << time) | (num >> (32 - time));
}

/**
 * Mix the bits of four 32-bits number
 *
 * @param[in]    (pass by reference) four 32-bits number
 * 
 * @param[out]   numbers after mix the bits (by add, xor, and rotate) with each other
 *
 */
static void update (uint32_t *w, uint32_t *x, uint32_t *y, uint32_t *z)
{
    uint32_t t1, t2, t3, t4; // temps value
    
    t1 = *w + *x;
    t4 = rotl(*z ^ t1, 16);
    t3 = *y + t4;
    t2 = rotl(*x ^ t3, 12);
    t1 += t2;
    t4 = rotl(t4 ^ t1, 8);
    t3 += t4;
    t2 = rotl(t2 ^ t3, 7);
    
    *w = t1;
    *x = t2;
    *y = t3;
    *z = t4;
}

/**
 *    Function to copy 64 bytes number into array of 16 32-bit numbers.
 *    Use the perm function to randomize the bits of these nubmers
 *  and pass it back to the array. 
 *
 * @param[in]    64 bytes in
 *
 * @param[out]   64 bytes out after randomize by perm function 
 *
 */
void perm (unsigned char *in, unsigned char * out)
{
    uint32_t a[16];
    
    // Copy 64 bytes from in into 16 32-bit number in array a 
    memcpy(a, in, sizeof(a));
    
    int i = 0;
    for (; i < 10; i++)
    {
        update(&a[0], &a[4], &a[8], &a[12]);
        update(&a[1], &a[5], &a[9], &a[13]);
        update(&a[2], &a[6], &a[10], &a[14]);
        update(&a[3], &a[7], &a[11], &a[15]);
        update(&a[0], &a[5], &a[10], &a[15]);
        update(&a[1], &a[6], &a[11], &a[12]);
        update(&a[2], &a[7], &a[8], &a[13]);
        update(&a[3], &a[4], &a[9], &a[14]);
    }

    // copy the number from array a back to out (64 bytes in total).
    memcpy(out, a, sizeof(a));
}

/*------------------------hash---------------------------------------------------*/
static void xor(unsigned char *dst, unsigned char *src, int num_bytes)
{
    int i = 0;
    for (i; i < num_bytes; i++)
    {
        dst[i] ^= src[i];
    }
}

/**
 *    Function to hash m bytes input and return a 32 bytes output using
 *     perm 
 *
 * @param[in]    m bytes input
 *
 * @param[out]   32 bytes output
 *
 */
void permhash(unsigned char *m, int mbytes, unsigned char *res)
{
    int r = 32, c = 32, b = 32;
   
    unsigned char temp[r]; // create a temp to save r bytes from m instead of
                           // array because if the size is to large, it will
                            // cause integer overflow and buffer overflow
    int i = 0;
    
    unsigned char block[r+c];
    for (i = 0; i < r+c; i++)
    {
        block[i] = (unsigned char)(i+1);
    }

    // while there are mbytes can still be divided into r-byte segment
    while (mbytes >= r)                         
    {
        memcpy(temp, m, r); // copy r bytes from m to temp
        xor(block, temp, r); // xor first rate bytes of block with
                                // rate-byte in temp 
        perm(block, block);  // perm  block value
        mbytes -= r;  
        m += r;
    }
    
    memcpy(temp, m, mbytes); 
    temp[mbytes] = 0b10000000; // pad 10* to the remaining bytes
    
    for (i = mbytes + 1; i < r; i++)
        temp[i] = (unsigned char)0;
    
    xor(block, temp, r);
    perm(block, block);

    
    if (b > r) // if the number of bytes of output is greater than rate
    {
        perm(block, block); // perm block 1 more time
        memcpy(res, block, b-r); // copy the first b-r bytes of block to result
    }
    else // if not
        memcpy(res, block, r); // copy first rate bytes of block into result
}


/*------------------------permctr------------------------------------------------*/
// in and out are pointers to 64-byte buffers
// key points to 0-to-64-byte buffer, kbytes indicates it's length
// Computes: (perm(in xor key) xor key) and writes 64-bytes to out
//           key has (64 - kbytes) zero bytes appended to its end
static void perm_bc(unsigned char *in, unsigned char *out,
                       unsigned char *key, int kbytes)
{
    unsigned char perm_in[64];
    unsigned char perm_out[64];
    
    // copy in to perm_in
    memcpy(perm_in, in, 64);
    
    int i = 0;
    // xor key to perm_in
    for(;i < kbytes; i++)
    {
        perm_in[i] = perm_in[i] ^ key[i];
    }
    
    // perm perm_in to perm_out
    perm(perm_in, perm_out);
    
    int j = 0;
    // xor key to perm_out
    for (; j < kbytes; j++)
    {
        perm_out[j] = perm_out[j] ^ key[j];
    
    }
    // copy perm_out to out
    memcpy(out, perm_out, 64);

    
}

static void increment (unsigned char *block)
{
    int i = 63;
    do
    {
        block[i] += 1;
        i -= 1;
    }while (block[i+1] == 0);
}

void permctr(unsigned char *in,    // Input buffer
                unsigned char *out,   // Output buffer
                int nbytes,           // Number of bytes to process
                unsigned char *block, // A 64-byte buffer holding IV+CTR
                unsigned char *key,   // Key to use. 16-32 bytes recommended
                int kbytes)           // Number of key bytes <= 64 to use.
{
    unsigned char buf[64];
    int len = 0;
    while (nbytes > 0)
    {
        // perm_bc block to buf
        perm_bc(block, buf, key, kbytes);
        
        // len = min(nbytes, 64)
        if (nbytes < 64)
            len = nbytes;
        else
            len = 64;
        
        // xor len bytes of buf with in to out
        int i = 0;
        for (; i < len; i++)
        {
            out[i] = in[i] ^ buf[i];
        }
        
        // increment block
        //increment(block);
          i = 63;
    do
    {
        block[i] += 1;
        i -= 1;
    }while (block[i+1] == 0);
        in = in + len;
        out = out + len;
        nbytes = nbytes - len;
    }
}

/*perm siv*/
void permsiv_encrypt(unsigned char *k, int kbytes,
                        unsigned char *n, int nbytes,
                        unsigned char *p, int pbytes,
                        unsigned char *siv, unsigned char *c)
{
    int i = 0;
    unsigned char k_temp[32];
    memcpy(k_temp, k, kbytes);
    // padded zero to 32 bytes
    for (i = kbytes; i < 32; i++)
    {
        k_temp[i] = (unsigned char)0;
    }

    unsigned char n_temp[16];
    memcpy(n_temp, n, nbytes);
    // n padded zero to 16 bytes 
    for (i = nbytes; i < 16; i++)
    {
        n_temp[i] = (unsigned char)0;
    }

    unsigned int size = kbytes + nbytes + pbytes;   
    
    //allocate the to_hash array
    unsigned char* to_hash = (unsigned char *) malloc(48 + pbytes);
    
    // padd k bytes into to_hash
    for (i = 0; i < 32; i++)
    {
        to_hash[i] = k_temp[i];
    }

    // Padd nbytes into to_hash
    for (i = 0; i < 16; i++)
    {
        to_hash[kbytes + i] = n_temp[i];
    }
    
    // Padd pbytes into to_hash
    for (i = 0; i < pbytes; i++)
    {
        to_hash[kbytes + nbytes + i] = p[i];
    } 

    // hash to_hash array by permhash
    permhash(to_hash, size, to_hash);
    
    // copy first 16 bytes from to_hash to siv
    memcpy(siv, to_hash, 16);
    
    unsigned char block[64];
    // padd siv into block
    memcpy(block, siv, 16);
    for (i = 16; i < 64; i++)
    {
        block[i] = (unsigned char)0;
    }
    permctr(p, c, pbytes, block, k, kbytes);
    
    // free the allocated array
    free(memset(to_hash, 0, 48 + pbytes));
}   


int permsiv_decrypt(unsigned char *k, int kbytes,
                       unsigned char *n, int nbytes,
                       unsigned char *c, int cbytes,
                       unsigned char *siv, unsigned char *p)
{
    unsigned char block[64];
    // padd siv into block
    memcpy(block, siv, 16);
    
    int i = 0;
    // padd 48 bytes zero to block
    for (i = 16; i < 64; i++)
    {
        block[i] = (unsigned char)0;
    }
    
    permctr(c, p, cbytes, block, k, kbytes);
    
    unsigned int size = kbytes + nbytes + cbytes;
    
    // allocated the to_hash function
    unsigned char *to_hash = (unsigned char*) malloc(48+cbytes);
    unsigned char k_temp[32];
    memcpy(k_temp, k, kbytes);
    // padd 0 to k array
    for (i = kbytes; i < 32; i++)
    {
        k_temp[i] = (unsigned char)0;
    }

    unsigned char n_temp[16];
    memcpy(n_temp, n, nbytes);
    // padd 0 to n array
    for (i = nbytes; i < 16; i++)
    {
        n_temp[i] = (unsigned char)0;
    }

    // padd k bytes to to_hash
    for (i = 0; i < 32; i++)
    {
        to_hash[i] = k_temp[i];
    }

    // Pdd n bytes to to_hash
    for (i = 0; i < 16; i++)
    {
        to_hash[kbytes + i] = n_temp[i];
    }
    
    // Padd cbytes to to_hash
    for (i = 0; i < cbytes; i++)
    {
        to_hash[kbytes + nbytes + i] = p[i];
    } 
    
    unsigned char temp[16];
    permhash(to_hash, size, to_hash);
    
    // copy the first 16 bytes from to_hash to temp
    memcpy(temp, to_hash, 16);
    
    // if siv == first 16 bytes of the to_hash
    if (memcmp(siv, temp, 16) == 0)
        return 0;
    else
        return -1;
    
    // free the allocated array
    free(memset(to_hash, 0, 48+cbytes));
}




/******************************************************************************
 *                              Encrypt File                                   *
 ******************************************************************************
 */
void encrypt_file(char* file_in_path, char* file_out_path)
{
    struct stat st_file;
    if (stat(file_in_path, &st_file) == -1)
    {
        fprintf(stderr, "Error when open file %s \n",
                file_in_path);
        exit(1);
    }
    
    // calculate file size
    size_t size = st_file.st_size * sizeof(unsigned char);
    
    // copy file byte into array (plain text):
    unsigned char* p = (unsigned char*)malloc(size);
    FILE* file = fopen(file_in_path, "r");
    fread(p, 1, size, file);
    fclose(file);
    
    // create array to store encrypted byte:
    unsigned char* c = (unsigned char*)malloc(size);
    
    // Define key, nonce, and siv:
    unsigned char k[8] = {1,2,3,4,5,6,7,8};
    unsigned char n[8] = {1,2,3,4,5,6,7,8};
    unsigned char siv[16];
    
    // Encrypt the data:
    permsiv_encrypt(k,8,n,8,p,size,siv, c);
    
    // Write encrypted data to output file:
    FILE* out = fopen(file_out_path, "w");
    fwrite(c, 1, size, out);
    fclose(out);

    free(p);
    free(c);
}

void decrypt_file (char* file_in_path, char* file_out_path)
{
    struct stat st_file;
    if (stat(file_in_path, &st_file) == -1)
    {
        fprintf(stderr, "Error when open file %s \n",
                file_in_path);
        exit(1);
    }
    
    // calculate file size
    size_t size = st_file.st_size * sizeof(unsigned char);
    
    // copy file byte into array (plain text):
    unsigned char* c = (unsigned char*)malloc(size);
    FILE* file = fopen(file_in_path, "r");
    fread(c, 1, size, file);
    fclose(file);
    
    // create array to store encrypted byte:
    unsigned char* p = (unsigned char*)malloc(size);
    
    // Define key, nonce, and siv:
    unsigned char k[8] = {1,2,3,4,5,6,7,8};
    unsigned char n[8] = {1,2,3,4,5,6,7,8};
    unsigned char siv[16];
    
    // Encrypt the data:
    int result = permsiv_decrypt(k,8,n,8,c,size,siv, p);
    
    // this is for debug
    if (result == 0)
        printf("success\n");
    else
        printf("fail");
    
    // Write encrypted data to output file:
    FILE* out = fopen(file_out_path, "w");
    fwrite(p, 1, size, out);
    fclose(out);

    free(p);
    free(c);
}

/******************************************************************************
 *                               Hide Chunk                                   *
 ******************************************************************************
 */
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

    size_t size_of_result = (len + chunk_size + 3) * sizeof(unsigned char); // adding 3 bytes to length so I can include my own trailer after the jpeg trailer
 	unsigned char *data_to_write = (unsigned char*) malloc(size_of_result);
		
	//read jpg into buffer 
	FILE* jpg=fopen(path_to_jpeg, "r");

	
	char tmpBuffer[512];
    int j = 0;
    int end_marker = -1; //the offset of the first free byte of memory after the final FFD9 (and the added  0x24 0xFF 0xD9 trailer we put in)

    //copy all of the data from the jpeg into the buffer
    while (fread(tmpBuffer, 1, 512, jpg))
    {
        for (int i = 0; i < 512; i++)
        {
            data_to_write[j] = tmpBuffer[i];
            if(j > 0)
            {
                if(data_to_write[j-1] == 0xff && data_to_write[j] == 0xd9) //  make note every time we find FFD9 
                {
                    end_marker = j; // store as end_marker until we find the last one
                }
            }
            j++; 
        }
    }
    //after all the data has been copied into the buffer, figure out where the final FFD9 was and start writing data there.

    size_of_result=( (end_marker+1) + 3 + chunk_size) * sizeof(unsigned char); // size of result = <bytes of data from jpeg> + <3 bytes for our trailer> + <size of data to hide>

    //write our custom trailer in so the extractor knows where the data begins
    data_to_write[end_marker+1]=0x24;
    data_to_write[end_marker+2]=0xFF;
    data_to_write[end_marker+3]=0xD9;
    j = end_marker+4;

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

/******************************************************************************
 *                               Extract Chunk                                *
 ******************************************************************************
 */
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
            if(jpg[j-4] == 0xff &&jpg[j-3] == 0xd9 &&jpg[j-2] == 0x24 && jpg[j-1] == 0xff && jpg[j] == 0xd9) // found the jpeg trailer
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
    //free(jpg);
    fclose(jpgf);
    *size = ((int) size_of_chunk);
    return chunk;
}

/******************************************************************************
 *                               Split File                                   *
 ******************************************************************************
 */
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
	printf("file size: %ld\n", file_size);
	printf("chunk size: %ld\n", chunk_size);
    
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
	
	//printf("file count: %d\n", file_count);
    // array to store full path:
    char full_path[300];
    int done = 0;
    
	int full_size = 0;
	
    rewinddir(dir_path);
	
	int size_of_chunk;
    int *size = &size_of_chunk;
	
	unsigned char* data = NULL;
	unsigned char* file;
	
	
	int i = 0;
	int j = 0;
	while (((entry = readdir(dir_path)) != NULL) && (!done))
    {

        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
            // create new full path
            strcpy(full_path, jpg_directory);
            strcat(full_path, "/");
            strcat(full_path, entry->d_name);
            
            // if there are still byte to read from encryption file:
			//printf("%s\n", full_path);
		
			data = extract_chunk(full_path, size);
			
			//printf("%d", size_of_chunk);
			
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
	
	//printf("%d", full_size);
	
	FILE* out = fopen(encrypt_reassemble_path, "w");
	fwrite(file, 1, full_size, out);
	fclose(out);
	
	
	free(file);
    closedir(dir_path); 
}
