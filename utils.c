#define _GNU_SOURCE
#define VIRT_ADDRESS 0x600000000000
#define CHUNK_METADATA_SIZE 9

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include "scm.h"

/* create crc table*/
void create_crc_table(unsigned int *table)
{
    int i,j;
    for (i = 0; i < 256; i++)
    {
        unsigned int crc = i;
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x1)
            {
                crc = (crc >> 1) ^ 0xEDB88320;
            }
            else
            {
                crc = (crc >> 1);
            }
        }
        table[i] = crc;
    }
}

/* crc checksum utlity function*/
unsigned int crc32(unsigned char *data, unsigned int length){
    unsigned int table[256];
    unsigned int crc;
    int i;

    create_crc_table(table);
    crc = 0xFFFFFFFF;
    for (i = 0; i < (int)length; i++)
    {
        crc = table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

/* create file on truncate*/
int create_file(const char *pathname){
    FILE *fd;
    char cmd[200]; /* Make sure it's large enough to hold the entire command */ 
    strcpy(cmd, "dd if=/dev/zero bs=4096 count=10000 of=");
    strcat(cmd, pathname);
    fd = popen(cmd, "w");
    if (!fd)
    {
        TRACE("Failed pipe open execution!!");
        return -1;
    }
    pclose(fd);
    return 0;
}

void print(char * s){
    FILE *f;
    f = fopen("output.txt", "a");
    if (f == NULL) {
        TRACE("Failed to open file");
        return;
    }
    fprintf(f,"%s\n",s);
    fclose(f);
}

void printmem(void * p){
    FILE *f;
    f = fopen("output.txt", "a");
    if (f == NULL) {
        TRACE("Failed to open file");
        return;
    }
    fprintf(f,"%p\n",p);
    fclose(f);
}

void printsize(size_t size){
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%lu", size);
    print(buffer);
}

/* get size from memory to load into scm struct*/
size_t get_size(size_t * address){
    size_t size;
    char buffer[200];
    size = *address;
    snprintf(buffer, sizeof(buffer), "%lu", size);
    print("Getting size =");
    print(buffer);
    print("At address =");
    printmem((void*)address);
    print("------------------------------------------");
    return size;
}

/* put size into memory on 1st time init or whenever size change has occured*/
void set_size(size_t * address, size_t size){
    char buffer[200];
    *address = size;
    snprintf(buffer, sizeof(buffer), "%lu", size);
    print("Storing size =");
    print(buffer);
    print("At address =");
    printmem((void*)address);
    print("------------------------------------------");
    return;
}

int validate_signature(uint8_t *address){
    uint8_t actual_signature[3] = {0xAA, 0xBB, 0xCC};
    uint8_t read_signature[3];
    char buffer[30];
    int i,comparison_result;
    print("Signature values");
    for (i = 0; i < 3; i++) {
        read_signature[i] = *address;
        snprintf(buffer, sizeof(buffer), "%d", read_signature[i]);
        print(buffer);
        address++;
    }
    comparison_result = memcmp(actual_signature, read_signature, 3);
    if(comparison_result==0){
        return 0;
    }
    else{
        return -1;
    }
}

size_t string_length(const char* str){
    size_t length = 0;

    /* Iterate through the string using a pointer */ 
    while (*str != '\0') {
        length++;
        str++; /* Move the pointer to the next character */ 
    }
    return length+1;
}

/* add a chunk and metadata at given location*/
int add_chunk(char* address, size_t old_chunk_size, size_t size){
    size_t *size_t_ptr;
    ssize_t new_chunk_size;
    uint8_t *uint8_t_ptr;
    new_chunk_size = (ssize_t)(old_chunk_size-size-(size_t)CHUNK_METADATA_SIZE);
    if(new_chunk_size<=0){ /* if this condition is true, then it means old_chunk_size */
        return -1;         /* is sufficient to accomodate size but not sufficient to */
    }                       /* accomodate metadata of next chunk*/
    print("Adding new metadata");
    print("At address");
    printmem(address);
    size_t_ptr = (size_t*)address;
    /* Set new chunk size*/
    set_size(size_t_ptr, new_chunk_size);
    size_t_ptr+=1;
    uint8_t_ptr = (uint8_t*)size_t_ptr;
    /* Set used flag*/
    *uint8_t_ptr = 0;
    return 0;
}

/* check if a chunk is used*/
bool check_used(uint8_t * address){
    uint8_t used;
    char buffer[200];
    used = *address;
    snprintf(buffer, sizeof(buffer), "%d", used);
    print("Getting used flag =");
    print(buffer);
    print("At address =");
    printmem((void*)address);
    print("------------------------------------------");
    if(used==0){
        return false;
    }
    else{
        return true;
    }
}

void set_used(uint8_t * address, uint8_t value){
    char buffer[200];
    *address = value;
    snprintf(buffer, sizeof(buffer), "%d", value);
    print("Storing flag =");
    print(buffer);
    print("At address =");
    printmem((void*)address);
    print("------------------------------------------");
    return;
}

void* increment_by_chunk_metadata(void* address){
    size_t *size_t_ptr;
    uint8_t *uint8_t_ptr;
    size_t_ptr = (size_t*)address;
    size_t_ptr+=1;
    uint8_t_ptr = (uint8_t*)size_t_ptr;
    uint8_t_ptr+=1;
    return (void*)uint8_t_ptr;
}