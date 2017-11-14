/**
 * CÓDIGO DEL PROFESOR CODAGNONE, levemente modificado 
 */

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h> 
#include <string.h>
#include <stdio.h>
#include "buffer.h"

inline void buffer_reset(buffer *b) {
    b->read  = b->data;
    b->write = b->data;
}

void buffer_init(buffer *b, const size_t n, char *data) {
    b->data = data;
    buffer_reset(b);
    b->limit = b->data + n;
}


int buffer_can_write(buffer *b) {
    return b->limit - b->write > 0;
}

char * buffer_write_ptr(buffer *b, size_t *nbyte) {
    assert(b->write <= b->limit);
    *nbyte = b->limit - b->write;
    return b->write;
}

int buffer_can_read(buffer *b) {
    return b->write - b->read > 0;
}

char * buffer_read_ptr(buffer *b, size_t *nbyte) {
    assert(b->read <= b->write);
    //check if end of command appears ('\0') and read until that
    size_t comm_end = 0;
    char *aux_ptr = b->read;
    for(int i = 0; aux_ptr <= b->write && *aux_ptr!='\0'; aux_ptr++){
        comm_end++;
    }
    if(comm_end < (size_t)(b->write - b->read)) {
        *nbyte = comm_end;
    }
    else {
        *nbyte = b->write - b->read;
    }
    return b->read;
}

char * buffer_read_ptr_for_client(buffer *b, size_t *nbyte){
    assert(b->read <= b->write);
    //check if end of command appears ('\0') and read until that
    size_t comm_end = 0;
    char *aux_ptr = b->read;
    for( ; aux_ptr < b->write && *aux_ptr!='\n'; aux_ptr++){
        comm_end++;
    }
    //Agrego el \n si hay un comando completo
    if(*aux_ptr == '\n'){
        comm_end++;
    }
    *nbyte = comm_end;
    return b->read;
}

void buffer_write_adv(buffer *b, const ssize_t bytes) {
    if(bytes > -1) {
        b->write += (size_t) bytes;
        assert(b->write <= b->limit);
    }
}

void buffer_read_adv(buffer *b, const ssize_t bytes) {
    if(bytes > -1) {
        b->read += (size_t) bytes;
        //test
        if(b->read > b->write)
            b->read = b->write;
        assert(b->read <= b->write);

        if(b->read == b->write) {
            // compactacion poco costosa
            buffer_compact(b);
        }
    }
}

char buffer_read(buffer *b) {
    char ret;
    if(buffer_can_read(b)) {
        ret = *b->read;
        buffer_read_adv(b, 1);
    } else {
        ret = 0;
    }
    return ret;
}

void buffer_write(buffer *b, char c) {
    if(buffer_can_write(b)) {
        *b->write = c;
        buffer_write_adv(b, 1);
    }
}

void buffer_compact(buffer *b) {
    if(b->data == (char*)b->read) {
        // nada por hacer
    } else if(b->read == b->write) {
        b->read  = b->data;
        b->write = b->data;
    } else {
        const size_t n = b->write - b->read;
        memmove(b->data, b->read, n);
        b->read  = b->data;
        b->write = b->data + n;
    }
}

int size_to_read(buffer *b){
    return b->limit - b->read;
}

int size_to_write(buffer *b){
    return b->limit - b->write;
}