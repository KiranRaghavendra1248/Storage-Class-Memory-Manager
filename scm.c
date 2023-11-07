/*
Useful Links :
https://stackoverflow.com/questions/60558310/how-to-add-subtract-memory-addresses-in-c
https://stackoverflow.com/questions/6338162/what-is-program-break-where-does-it-start-from-0x00
*/

#define _GNU_SOURCE
#define VIRT_ADDRESS 0x600000000000
#define CHUNK_METADATA_SIZE 9

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include "scm.h"
#include "utils.h"

/**
 * Needs:
 *   fstat()
 *   S_ISREG()
 *   open()
 *   close()
 *   sbrk()
 *   mmap()
 *   munmap()
 *   msync()
 */

/* research the above Needed API and design accordingly */

struct scm
{
    int fd;
    size_t length, size, metadata; /* total file size, currently occupied size, metadata size in bytes*/
    char *addr, *size_ptr, *base;
};

struct scm *scm_open(const char *pathname, int truncate)
{
    int fd, flag;
    struct scm *scmptr;
    struct stat finfo;
    size_t file_length;
    char *curr_brk;
    void *map_ptr;
    uint8_t signature[3] = {0xAA, 0xBB, 0xCC}; /* signature to encode in the start of memory*/
    uint8_t *uint8_t_ptr;
    size_t *size_t_ptr;

    /* Create new file on truncate*/
    if (truncate)
    {
        /* Delete old output file*/
        if (remove("output.txt") == 0)
        {
            print("Existing output.txt file deleted successfully");
        }
        else
        {
            print("Output file deletion failed");
        }
        if (-1 == create_file(pathname))
        {
            TRACE("Failed file creation!!");
            return NULL;
        }
    }
    /* Allocate memory for scm pointer*/
    scmptr = (struct scm *)malloc(sizeof(struct scm));
    if (!scmptr)
    {
        TRACE("Failed malloc for SCM struct!!");
        return NULL;
    }
    /* Open file*/
    fd = open(pathname, O_RDWR);
    if (fd < 0)
    {
        TRACE("Failed file opening!!");
        FREE(scmptr);
        return NULL;
    }
    /* Get file statistics*/
    if (fstat(fd, &finfo) < 0)
    {
        TRACE("Failed execution fstat!!");
        FREE(scmptr);
        return NULL;
    }
    /* Sanity check for file*/
    if (0 == S_ISREG(finfo.st_mode))
    {
        TRACE("Failed sanity check!!");
        FREE(scmptr);
        return NULL;
    }
    file_length = (size_t)finfo.st_size;

    /* Sanity check for virtual memory start address*/
    curr_brk = sbrk(0);
    if (curr_brk >= (char *)VIRT_ADDRESS)
    {
        TRACE("Virtual memory start address is below break line");
        FREE(scmptr);
        return NULL;
    }

    /* mmap file and process virtual memory*/
    map_ptr = (void *)mmap((void *)VIRT_ADDRESS, file_length, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0);
    if ((map_ptr == MAP_FAILED) || (map_ptr != (void *)VIRT_ADDRESS))
    {
        TRACE("Failed execution mmap!!");
        FREE(scmptr);
        return NULL;
    }
    print("SCM Opening");
    /* If truncate, initlize scm attributes as it is a new file */
    if (truncate == 1)
    {
        print("Truncating existing file");
        scmptr->addr = (char *)map_ptr;
        scmptr->base = (char *)map_ptr;
        scmptr->size = 0;
        scmptr->fd = fd;
        scmptr->length = file_length;
        scmptr->metadata = 0;

        /* encode signature(3 bytes) to location and incement pointer */
        /* typecast to uint8_t to get byte wise access to memory */
        uint8_t_ptr = (uint8_t *)scmptr->base;
        memcpy(uint8_t_ptr, signature, 3);
        uint8_t_ptr += 3;
        scmptr->base = (char *)uint8_t_ptr;
        scmptr->metadata += 3 * (sizeof(uint8_t));

        /* encode size to specified location and increment pointer*/
        size_t_ptr = (size_t *)scmptr->base;
        scmptr->size_ptr = (char *)size_t_ptr;
        set_size(size_t_ptr, scmptr->size);
        size_t_ptr += 1;
        scmptr->base = (char *)size_t_ptr;
        scmptr->metadata += 1 * (sizeof(size_t));
        print("Metadata size");
        printsize(scmptr->metadata);

        /* add new chunk */
        flag = add_chunk(scmptr->base,scmptr->length - scmptr->metadata, 0);
        if(flag<0){
            TRACE("Failed to add chunk");
            FREE(scmptr);
            return NULL;
        }
        return scmptr;
    } /* Else validate signature, load size from file, init everything else */
    else
    {
        print("Loading from file");
        scmptr->addr = (char *)map_ptr;
        scmptr->base = (char *)map_ptr;
        scmptr->fd = fd;
        scmptr->length = file_length;
        scmptr->metadata = 0;

        /* validate signature*/
        uint8_t_ptr = (uint8_t *)scmptr->base;
        if (-1 == validate_signature(uint8_t_ptr))
        {
            TRACE("Garbage Values Detected in File");
            FREE(scmptr);
            return NULL;
        }
        uint8_t_ptr += 3;
        scmptr->base = (char *)uint8_t_ptr;
        scmptr->metadata += 3 * (sizeof(uint8_t));

        /* get size from memory*/
        size_t_ptr = (size_t *)scmptr->base;
        scmptr->size_ptr = (char *)size_t_ptr;
        scmptr->size = get_size(size_t_ptr);
        size_t_ptr += 1;
        scmptr->base = (char *)size_t_ptr;
        scmptr->metadata += 1 * (sizeof(size_t));
        print("Metadata size");
        printsize(scmptr->metadata);
        return scmptr;
    }
    
}

void *scm_malloc(struct scm *scm, size_t n)
{
    size_t *size_t_ptr, *traverse, chunk_size, *next_chunk_address, *curr_block_size_ptr=NULL;
    bool chunk_used, overwrite_size=false;
    print("Malloc occured for size =");
    printsize(n);
    scm->size += n;

    /* update size*/
    /* encode size to specified location*/
    size_t_ptr = (size_t *)scm->size_ptr;
    set_size(size_t_ptr, scm->size);

    /* find suitable chunk*/
    traverse = (size_t*)scm->base;
    while(traverse<((size_t*)(scm->base+(scm->length-scm->metadata)))){
        chunk_size = get_size(traverse);
        chunk_used = check_used((uint8_t*)(traverse+1));
        curr_block_size_ptr = traverse;
        traverse = (size_t*)increment_by_chunk_metadata(traverse);
        if((chunk_used==false)&&(chunk_size>=n)){
            next_chunk_address = traverse  + n;
            if(0==add_chunk((char*)next_chunk_address, chunk_size, n)){
                overwrite_size = true; /* this means  chunk has been partioned, so overwrite size of current chunk*/
            }
            else{
                overwrite_size = false; /* this means we had unsuccessful partion of current chunk -> give whole chunk for current request*/
            }
            break;
        }
        else{
            traverse = traverse + chunk_size;
        }
    }
    if(traverse>((size_t*)(scm->base+(scm->length-scm->metadata)))){
        /*  crossed memory limits & did not find a suitable node*/
        return NULL;
    }
    else{
        if(overwrite_size){
            set_size(curr_block_size_ptr, n);
        }
        set_used((uint8_t*)(curr_block_size_ptr+1), 250);
        print("Dynamic mem allocated at address :");
        printmem(traverse);
        print("------------------------------------------");
        return (void*)traverse;
    }
}

void *scm_mbase(struct scm *scm){
    return increment_by_chunk_metadata(scm->base);
}

size_t scm_capacity(const struct scm *scm)
{
    return scm->length;
}

size_t scm_utilized(const struct scm *scm)
{
    return scm->size;
}

void scm_close(struct scm *scm)
{
    print("SCM Closing");
    /* performs msync and munmap*/
    if (-1 == close(scm->fd))
    {
        TRACE("Failed closing file!!");
        FREE(scm);
        return;
    }
    if (-1 == msync((void *)scm->addr, scm->length, MS_SYNC | MS_INVALIDATE))
    {
        TRACE("Failed msync execution!!");
        FREE(scm);
        return;
    }
    if (-1 == munmap((void *)scm->addr, scm->length))
    {
        TRACE("Failed munmap execution!!");
        FREE(scm);
        return;
    }
    FREE(scm);
}

char *scm_strdup(struct scm *scm, const char *s)
{
    size_t length;
    char *new_string;

    length = string_length(s);
    new_string = (char *)scm_malloc(scm, length);
    if(!new_string){
        TRACE("Strdup failed");
        return NULL;
    }
    memcpy(new_string, s, length);
    return new_string;
}

void scm_free(struct scm *scm, void *p){
    size_t *size_t_ptr, chunk_size, *total_size_ptr, total_size;
    uint8_t *uint8_t_ptr;

    /* set used = false*/
    uint8_t_ptr = (uint8_t*)p;
    uint8_t_ptr-=1;
    set_used(uint8_t_ptr, 0);
    /* get size*/
    size_t_ptr = (size_t*)uint8_t_ptr;
    size_t_ptr-=1;
    chunk_size = get_size(size_t_ptr);

    /* Log shit*/
    print("Freeing size");
    printsize(chunk_size);
    print("Freeing dynamic mem allocated at");
    printmem(p);
    print("------------------------------------------");

    /* remove chunk size from total size*/
    total_size = scm->size;
    total_size_ptr = (size_t *)scm->size_ptr;
    scm->size-=chunk_size;
    /* Store new size to file as well*/
    set_size(total_size_ptr, total_size-chunk_size);
    print("Setting new size used");
    printsize(total_size-chunk_size);
    print("At address");
    printmem(total_size_ptr);
}

void scm_free_const(struct scm *scm, const void *p){
    size_t *size_t_ptr, chunk_size, *total_size_ptr, total_size;
    uint8_t *uint8_t_ptr;

    /* set used = false*/
    uint8_t_ptr = (uint8_t*)p;
    uint8_t_ptr-=1;
    set_used(uint8_t_ptr, 0);
    /* get size*/
    size_t_ptr = (size_t*)uint8_t_ptr;
    size_t_ptr-=1;
    chunk_size = get_size(size_t_ptr);

    /* Log shit*/
    print("Freeing dynamic mem allocated for string of size");
    printsize(chunk_size);
    print("Stored at");
    printmem_const(p);

    /* remove chunk size from total size*/
    total_size = scm->size;
    total_size_ptr = (size_t *)scm->size_ptr;
    scm->size-=chunk_size;
    /* Store new size to file as well*/
    set_size(total_size_ptr, total_size-chunk_size);
    print("Setting new size used");
    printsize(total_size-chunk_size);
    print("At address");
    printmem(total_size_ptr);
}
