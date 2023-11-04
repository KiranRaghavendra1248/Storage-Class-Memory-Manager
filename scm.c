/**
 * Tony Givargis
 * Copyright (C), 2023
 * University of California, Irvine
 *
 * CS 238P - Operating Systems
 * scm.c
 */

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "scm.h"

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

struct scm{
    int fd;
    size_t length;
    char *size, *addr;
};

int create_file(char * pathname){
    FILE *fd;
    char buffer[2048];

    char cmd [] = "dd if=/dev/zero bs=4096 count=10000 of=";
    strcat(cmd,pathname);
    fd = popen(cmd,"w");
    if(!fd){
        TRACE("Failed pipe open execution!!");
        return -1;
    }
    while (fgets(buffer, 2048, fd) != NULL) {
        printf("%s", buffer);
    }
    pclose(fd);
    return 0;
}

struct scm *scm_open(const char *pathname, int truncate){
    int fd;
    struct scm *scmptr;
    struct stat finfo;

    /* Create new file on truncate*/
    if(truncate){
        create_file(pathname);
    }
    /* Allocate memory for scm pointer*/
    scmptr = (struct scm *)malloc(sizeof(struct scm));
    if(!scmptr){
        TRACE("Failed malloc for SCM struct!!");
        return NULL;
    }
    /* Open file*/
    fd = open(pathname, O_RDWR);
    if(fd<0){
        TRACE("Failed file opening!!");
        FREE(scmptr);
        return NULL;
    }
    /* Get file statistics*/
    if(fstat(fd, &finfo)<0){
        TRACE("Failed execution fstat!!");
        FREE(scmptr);
        return NULL;
    }
    /* Sanity check fof file*/
    if(0==S_ISREG(finfo.st_mode)){
        TRACE("Failed sanity check!!");
        FREE(scmptr);
        return NULL;
    }
    /* If truncate, reset scm attributes and reset size to 0 */
    /* Else load size from file */


}