#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
//#include <gcrypt.h>
#include <sys/stat.h>

/******************************************************************************
 *                            Ignore - Used functions                                     *
 ******************************************************************************
 */
/*---------------------------Perm 152----------------------------------------*/
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
 *    Use the perm152 function to randomize the bits of these nubmers
 *  and pass it back to the array. 
 *
 * @param[in]    64 bytes in
 *
 * @param[out]   64 bytes out after randomize by perm152 function 
 *
 */
void perm152 (unsigned char *in, unsigned char * out)
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
 *     perm 152
 *
 * @param[in]    m bytes input
 *
 * @param[out]   32 bytes output
 *
 */
void perm152hash(unsigned char *m, int mbytes, unsigned char *res)
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
        perm152(block, block);  // perm 152 block value
        mbytes -= r;  
        m += r;
    }
    
    memcpy(temp, m, mbytes); 
    temp[mbytes] = 0b10000000; // pad 10* to the remaining bytes
    
    for (i = mbytes + 1; i < r; i++)
        temp[i] = (unsigned char)0;
    
    xor(block, temp, r);
    perm152(block, block);

    
    if (b > r) // if the number of bytes of output is greater than rate
    {
        perm152(block, block); // perm block 1 more time
        memcpy(res, block, b-r); // copy the first b-r bytes of block to result
    }
    else // if not
        memcpy(res, block, r); // copy first rate bytes of block into result
}


/*------------------------perm152ctr------------------------------------------------*/
// in and out are pointers to 64-byte buffers
// key points to 0-to-64-byte buffer, kbytes indicates it's length
// Computes: (perm152(in xor key) xor key) and writes 64-bytes to out
//           key has (64 - kbytes) zero bytes appended to its end
static void perm152_bc(unsigned char *in, unsigned char *out,
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
    
    // perm152 perm_in to perm_out
    perm152(perm_in, perm_out);
    
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

void perm152ctr(unsigned char *in,    // Input buffer
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
        // perm152_bc block to buf
        perm152_bc(block, buf, key, kbytes);
        
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

/*perm152 siv*/
void perm152siv_encrypt(unsigned char *k, int kbytes,
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

    // hash to_hash array by perm152hash
    perm152hash(to_hash, size, to_hash);
    
    // copy first 16 bytes from to_hash to siv
    memcpy(siv, to_hash, 16);
    
    unsigned char block[64];
    // padd siv into block
    memcpy(block, siv, 16);
    for (i = 16; i < 64; i++)
    {
        block[i] = (unsigned char)0;
    }
    perm152ctr(p, c, pbytes, block, k, kbytes);
    
    // free the allocated array
    free(memset(to_hash, 0, 48 + pbytes));
}   


int perm152siv_decrypt(unsigned char *k, int kbytes,
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
    
    perm152ctr(c, p, cbytes, block, k, kbytes);
    
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
    perm152hash(to_hash, size, to_hash);
    
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
    perm152siv_encrypt(k,8,n,8,p,size,siv, c);
    
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
    int result = perm152siv_decrypt(k,8,n,8,c,size,siv, p);
    
    // this is for debug
    if (result == 0)
        printf("success");
    else
        printf("fail");
    
    // Write encrypted data to output file:
    FILE* out = fopen(file_out_path, "w");
    fwrite(p, 1, size, out);
    fclose(out);

    free(p);
    free(c);
}

int main()
{
    char* in = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/153-2020f.pdf";
    char* out = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/out.txt";
    char* out3 = "/mnt/c/Users/Admin/Desktop/CSC/CSC 153/out3.txt";
   
    encrypt_file(in, out);
    decrypt_file(out, out3);
     
    return 0;
}
