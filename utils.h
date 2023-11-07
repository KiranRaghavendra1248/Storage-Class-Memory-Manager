#ifndef _UTILS_H_
#define _UTILS_H_

#include "system.h"

void create_crc_table(unsigned int *table);
unsigned int crc32(unsigned char *data, unsigned int length);
int create_file(const char *pathname);
size_t get_size(size_t * address);
void set_size(size_t * address, size_t size);
int validate_signature(uint8_t *address);
size_t string_length(const char* str);
void print(char * s);
void printmem(void * p);
void printsize(size_t size);
int add_chunk(char* address, size_t old_chunk_size, size_t size);
bool check_used(uint8_t * address);
void set_used(uint8_t * address, uint8_t value);
void* increment_by_chunk_metadata(void* address);
void printmem_const(const void * p);
#endif /* _AVL_H_ */