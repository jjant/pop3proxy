/**
 * CÓDIGO DEL PROFESOR CODAGNONE, levemente modificado 
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdbool.h>
#include <stdint.h> 
#include <string.h>
#include <assert.h>
#include <unistd.h>  // size_t, ssize_t

typedef struct buffer buffer;
struct buffer {
    char *data;

    /** límite superior del buffer. inmutable */
    char *limit;

    /** puntero de lectura */
    char *read;

    /** puntero de escritura */
    char *write;
};

/**
 * inicializa el buffer sin utilizar el heap
 */
void buffer_init(buffer *buffer, const size_t n, char *data);

/**
 * Retorna un puntero donde se pueden escribir hasta `*nbytes`.
 * Se debe notificar mediante la función `buffer_write_adv'
 */
char * buffer_write_ptr(buffer *buffer, size_t *nbyte);

void buffer_write_adv(buffer *buffer, const ssize_t bytes);

char * buffer_read_ptr(buffer *buffer, size_t *nbyte);

char * buffer_read_ptr_for_client(buffer *buffer, size_t *nbyte);

void buffer_read_adv(buffer *buffer, const ssize_t bytes);

/**
 * obtiene un byte
 */
char buffer_read(buffer *buffer);

/** escribe un byte */
void buffer_write(buffer *buffer, char c);

/**
 * compacta el buffer
 */
void buffer_compact(buffer *buffer);

/**
 * Reinicia todos los punteros
 */
void buffer_reset(buffer *buffer);

/** retorna true si hay bytes para leer del buffer */
int buffer_can_read(buffer *b);

/** retorna true si se pueden escribir bytes en el buffer */
int buffer_can_write(buffer *b);

/** retorna es espacio que hay para leer y escribir en el buffer */
int size_to_read(buffer *b);

int size_to_write(buffer *b);

#endif