/**

    Copyright (c) 2026  Jonatan Gardell
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

    * Neither the name of the copyright holders nor the names of
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

/**
    Q_alloc provides allocation functions for small memory systems with inbuilt
    length specifier. Instead of using a linked freelist, the chunk headers
    indicates if a block is in use or free. Most significant bit set indicates
    a free chunk. The chunk length indicator is always the exact length of the
    chunk and can therefore be used as an indicator, and saves space when
    storing strings since the \0 terminator can be be omitted.
    Length is provided by the function q_size().

    Overhead is low, from one byte per chunk to sizeof(size_t) bytes.
    Two size_t global vars is needed. They can be replaced by __heap_start and
    __heap_end on systems like AVR. Memory MUST be initialized with q_initmem()

    Developed foremost for AVR, but also tested to work on AMD64.

    qalloc.h
*/


#ifndef __QALLOC_H__
#define __QALLOC_H__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/**
    Q_STR_... defines the maximum chunk len. If the data to be stored has a short length, a
    shorter chunk len saves RAM by decreasing overhead storage.
*/

//#define Q_STR_127       //int8_t pointers, max string length 127          (overhead: 1 byte per chunk)
//#define Q_STR_32k       //int16_t pointers, max string length 32767         (overhead: 2 bytes per chunk)
//#define Q_STR_2G        //int32_t pointers, max string length 2147483647. (overhead: 4 bytes per chunk) (Nonsensical on AVR)
//#define Q_STR_8E        //int64_t pointers, max string length 8 EB.       (overhead: 8 bytes per chunk) (Nonsensical on AVR)
#define Q_STR_NATIVE    //size_t pointers, max string length size_t/2-1   (overhead: sizeof(size_t) bytes per chunk)


//#define _Q_DEBUG_               // Enable misc debug functions and exposes __q_collect()
#define _Q_EXTRA_               // Enable q_get_freemem()
#define _Q_STRFUNCS_            // Enable q_storestr() and q_getstr()

//#define _Q_AVR_               // Protect stack in AVR


#ifdef _Q_AVR_
extern volatile uint8_t __SP_L__, __SP_H__;
#define STACK_POINTER() ((char*)((uint16_t)__SP_H__ << 8 | (uint16_t)__SP_L__))
#define __Q_MALLOC_MARGIN   32
#endif


/*
    Free pointer ptr previously allocated with q_malloc/q_calloc/q_realloc
*/
void q_free(const void *ptr);

/*
    Init memory. Q_malloc needs a map in memory of free space between
    addresses start and stop.
    Examples: q_initmem(____malloc_heap_start, __malloc_heap_end);
    q_initmem((size_t)&blob, (size_t)&blob + sizeof(blob) - 1);
*/
bool q_initmem(size_t start, size_t end);

/*
    q_malloc()
    Works like malloc() and returns a pointer to memory within the
    address __q_heap_start.addr and __q_heap_end.addr.
    Returns null pointer if allocation fails
*/
void *q_malloc(size_t len);

/*
    Returns the size of allocation pointed by ptr. This length is
    exactly the same as passed by the len parameter in q_alloc()
*/
size_t q_size(const void *ptr);

/*
    Re-allocates space for ptr of len. Works like realloc() with the
    exception that a null length returns a valid pointer to a zero length
    allocation. Returns null pointer if allocation fails. If ptr was a
    valid pointer, it still is even if allocation fails.
*/
void *q_realloc(void *ptr, size_t len);

/*
    Works like calloc().
*/
void *q_calloc(size_t __nele, size_t __size);

/*
    Store a normal null-terminated string without ending \0 to conserve
    space. Use q_size() to get length of string.
    Returns null pointer if allocation failed.
*/
void *q_storestr(const char *str);

/*
    Retrieve string by appending a null (\0) byte. Dest must have space
    for string length + 1
*/
char *q_getstr(char* dest, const void *src);


#ifdef _Q_DEBUG_
extern size_t way;
void ptrcheck(size_t ptr, long unsigned int dbg);
void getways(size_t *w1, size_t *w2, size_t *w3, size_t *w4);
void q_listalloc(void);
size_t q_countalloc(void) ;
size_t q_countzeroalloc(void) ;
size_t reladr(size_t addr);
void __q_collect(void);
#endif



#endif // __QALLOC_H__
