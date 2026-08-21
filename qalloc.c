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

#include <string.h>
#include "qalloc.h"
#ifdef _Q_DEBUG_
#endif
#if defined(_Q_DEBUG_) || defined(_Q_STRFUNCS_)
#include <stdio.h>
#endif

#ifdef Q_STR_127
    typedef uint8_t         q_ptr;
    typedef uint16_t        q_ptr_far;
#elifdef Q_STR_32k
    typedef uint16_t        q_ptr;
    typedef uint32_t        q_ptr_far;
#elifdef Q_STR_2G
    typedef uint32_t        q_ptr;
    typedef uint64_t        q_ptr_far;
#elifdef Q_STR_8E
    typedef uint64_t        q_ptr;
    typedef uint64_t        q_ptr_far;
#elifdef Q_STR_NATIVE
    typedef uint64_t        q_ptr;
    typedef uint64_t        q_ptr_far;
#endif // Q_STR_32k

#define Q_FREEMASK  ((uint64_t)0x80 << ((sizeof(q_ptr)-1ULL)*8ULL))
#define Q_MAXLEN    (Q_FREEMASK - 1ULL)

#define Q_REALLOC_TRIVIAL
//#define Q_REALLOC_STACK
//#define Q_REALLOC_TRYSTACK

#ifdef _Q_DEBUG_
    size_t way1, way2, way3, way4;
    size_t way;
#endif


typedef union {
    size_t  addr;
    q_ptr   *len;
} Uq_ptr;

Uq_ptr  __q_heap_start;
Uq_ptr  __q_heap_end;

static inline size_t __q_size_internal(const uint8_t *ptr){
    return (*( (q_ptr *)((uint8_t *)ptr - sizeof(q_ptr)) )) & ~Q_FREEMASK;
}

size_t q_size(const void *ptr){
    if(((size_t)ptr >= __q_heap_start.addr) && ((size_t)ptr <= __q_heap_end.addr)){
        return __q_size_internal(ptr);
    }
    return 0;
}

size_t __q_formatmem(size_t start, size_t end){

    size_t runlen = end - start;// - sizeof(q_ptr);
    Uq_ptr ptr;
    ptr.addr = start;
    //printf("\n");
    while(runlen > Q_MAXLEN){
        //printf("Ptr=%05x, rl=%lu\n", reladr(ptr.addr), runlen);
        *ptr.len = Q_MAXLEN | Q_FREEMASK;
        runlen -= (Q_MAXLEN + sizeof(q_ptr));
        ptr.addr += Q_MAXLEN + sizeof(q_ptr);
        if(ptr.addr >= __q_heap_end.addr){
          //  printf("runlen=0\n");
            runlen = 0;
            break;
        }
    }
    //printf("Brk-Ptr=%05x, rl=%lu\n", reladr(ptr.addr), runlen);
    if(runlen){
        //ptr.addr
        *ptr.len = runlen | Q_FREEMASK;
    }
    return ptr.addr;
}

bool q_initmem(size_t start, size_t end){
    __q_heap_start.addr = start;
    __q_heap_end.addr = end;
    #ifdef _Q_DEBUG_
    printf("\nRange:    %05lx ... %05lx", reladr(start), reladr(end));
    printf("\nFreemask: %016lx", (size_t)Q_FREEMASK);
    printf("\nMaxlen:   %016lx", (size_t)Q_MAXLEN);
    #endif
    if(start >= end - sizeof(q_ptr)){
        return false;
    }


    #ifdef _Q_AVR_
    if(end + __Q_MALLOC_MARGIN > STACK_POINTER()){
        return false;
    }
    #endif // _Q_AVR_
    if((end - start - sizeof(q_ptr) + 1) > Q_MAXLEN){   // Size of heap larger than MAXLEN -> we need to initialize every block
        __q_formatmem(start, end);// - Q_MAXLEN - sizeof(q_ptr) + 1);
    }else{                          // Size of heap is small enough for only one initialized pointer
        Uq_ptr ptr = __q_heap_start;
        *ptr.len = (end - start - sizeof(q_ptr) + 1) | Q_FREEMASK;
    }
    #ifdef _Q_DEBUG_
    q_listalloc();
    #endif // _Q_DEBUG_
    return true;
}

void __q_collect(void){
    Uq_ptr p = __q_heap_start;
    while(p.addr < __q_heap_end.addr){
        size_t runlen = -sizeof(q_ptr);
        size_t n = 0;
        Uq_ptr start = p;
        while(*p.len & Q_FREEMASK){                     // Compute total size of consecutive free blocks
            size_t l = (*p.len & ~Q_FREEMASK) + sizeof(q_ptr);
            runlen += l;

            if(p.addr + l < __q_heap_end.addr){         // Are we still in defined memory?
                p.addr += l;
            }else{
                break;
            }
            n ++;
        }

        if(n > 1){                                      // Do we have more than one block?
            if(runlen <= Q_MAXLEN){                     // Do free memory fit in one block?
                *start.len = runlen | Q_FREEMASK;       // Consolidate into just one block
            }else{                                      // So is the block the end of memory?
                while(runlen > Q_MAXLEN){               // Build length of consolidated free memory.
                    *start.len = Q_MAXLEN | Q_FREEMASK;
                    start.addr += Q_MAXLEN + sizeof(q_ptr);
                    if(start.addr > __q_heap_end.addr){
                        runlen = 0;
                        break;
                    }
                    runlen -= (Q_MAXLEN + sizeof(q_ptr));
                }
                if(runlen || (start.addr != p.addr)){   // Put a marker here?
                    if(start.addr <= __q_heap_end.addr){
                        *start.len = runlen | Q_FREEMASK;
                    }
                }
            }
        }else{
            p.addr += *p.len + sizeof(q_ptr);
        }
    }
}

#ifdef _Q_EXTRA_
size_t q_get_freemem(void){
    Uq_ptr ptr = __q_heap_start;

    size_t ret = 0;
    while (ptr.addr <= __q_heap_end.addr - sizeof(q_ptr)){
        q_ptr l = *ptr.len & ~Q_FREEMASK;
        if(*ptr.len & Q_FREEMASK){
            ret += l & ~Q_FREEMASK;
        }
        ptr.addr += l + sizeof(q_ptr);
    }
    return ret;
}
#endif

#ifdef _Q_STRFUNCS_
char *q_getstr(char* dest, const void *src){
    if(((size_t)src >= __q_heap_start.addr + sizeof(q_ptr)) && ((size_t)src <= __q_heap_end.addr)){
        #ifdef _Q_DEBUG_
        way = 55;
        #endif
        Uq_ptr p;
        p.addr = (size_t)src;
        p.addr -= sizeof(q_ptr);
        memmove(dest, src, *p.len);
        dest[*p.len] = 0;
        return dest;
    }
    #ifdef _Q_DEBUG_
    way = 56;
    #endif
    return 0;
}

void *q_storestr(const char *str){
    size_t l = strlen(str);
    //size_t n;
    char *r = q_malloc(l);
    if(r){
        #ifdef _Q_DEBUG_
        way = 44;
        #endif
        memmove(r, str, l);
    }
    return (void*)r;
}
#endif


void *q_malloc(size_t len) {
    //*dbg = 0;
    if(len > Q_MAXLEN){ // Requested length larger than MAXLEN
    #ifdef _Q_DEBUG_
        way = 70;
    #endif
        return 0;
    }
    __q_collect();


    Uq_ptr ptr = __q_heap_start;
    q_ptr mx = Q_MAXLEN + 1;
    Uq_ptr entryptr, postptr;
    q_ptr next;
    postptr = __q_heap_end;

    while(ptr.addr < __q_heap_end.addr){
        q_ptr l = *ptr.len & ~Q_FREEMASK;
        if(*ptr.len & Q_FREEMASK){
            if(l == len){   // Found free block of exact len
                *ptr.len = len;
                size_t ret = (ptr.addr + sizeof(q_ptr));
                #ifdef _Q_DEBUG_
                way = 71;
                #endif
                return (void*)ret;
            }else if(l >= len + (sizeof(q_ptr))){               // Need a free block higher than the length that have space for freelist entry
                if(l < mx){
                    mx = l;
                    entryptr = ptr;                             // This is the candidate position to insert
                    postptr = ptr;                              // A new free block
                    postptr.addr += len + sizeof(q_ptr);        // Create a new free block here
                    next = l - len - sizeof(q_ptr);             // That points to...
                    /**
                        [free]......................[next]
                        [entryptr]data[postptr].....[next]
                    */
                }
            }
        }
        ptr.addr = ptr.addr + l + sizeof(q_ptr);
    }

    if(mx == Q_MAXLEN + 1){   // Mem full or too fragmented
        #ifdef _Q_DEBUG_
        way = 72;
        #endif
        return 0;
    }

    #ifdef _Q_AVR_
    if(__q_heap_end + __Q_MALLOC_MARGIN > STACK_POINTER()){
        return 0;
    }
    #endif // _Q_AVR_

    // Create entry block
    *entryptr.len = len;
    if(postptr.addr < __q_heap_end.addr){
        *postptr.len = next | Q_FREEMASK;
    }

    size_t ret = entryptr.addr + sizeof(q_ptr);
    #ifdef _Q_DEBUG_
    way = 73;
    #endif
    return (void*)ret;
}

/*static inline void __q_unfree_internal(const void *ptr){
    *( (q_ptr *)((uint8_t *)ptr - sizeof(q_ptr)) ) &= ~Q_FREEMASK;
}*/

static inline void __q_free(const void *ptr){
    *( (q_ptr *)((uint8_t *)ptr - sizeof(q_ptr)) ) |= Q_FREEMASK;
}

void q_free(const void *ptr){
    //ptrcheck((size_t)ptr, 8);

    if(((size_t)ptr >= __q_heap_start.addr + sizeof(q_ptr)) && ((size_t)ptr <= __q_heap_end.addr)){
        if( *( (q_ptr *)((uint8_t *)ptr - sizeof(q_ptr)) ) & Q_FREEMASK ){
            // Already free
            #ifdef _Q_DEBUG_
            way = 98;
            #endif
        }else{
            __q_free(ptr);
            #ifdef _Q_DEBUG_
            way = 99;
            #endif
        }
    }
}

/**
    Difference from realloc() in glibc:
    q_realloc(ptr, 0) returns valid pointer to zero length object that can be passed to q_free()
    If q_realloc fails, it returns 0, but ptr is left untouched and if it's a valid pointer it can be passed to q_free()
*/



/**
    Trivial realloc. Creates a new allocation and copies old data, then frees old area. Worst case one memcpy.
*/
#ifdef Q_REALLOC_TRIVIAL
void *q_realloc(void *ptr, size_t len){
    //    size_t n;
    if(ptr == 0){       // If ptr is null, work just like q_malloc
        #ifdef _Q_DEBUG_
        way = 1;
        way4 ++;
        #endif
        return q_malloc(len);
    }

    if( ((len > Q_MAXLEN) || ((size_t)ptr < __q_heap_start.addr) || ((size_t)ptr > __q_heap_end.addr)) ){ // Is it a valid pointer to heap?
        #ifdef _Q_DEBUG_
        way = 2;
        #endif
        return 0;
    }

    if(len == __q_size_internal(ptr)){   // Len is same, just return it
        #ifdef _Q_DEBUG_
        way = 3;
        way4 ++;
        #endif
        return ptr;
    }

    /**
        First, check if we can shrink or extend the chunk in place. Depending
        on your data, you may want to swap order of the shrink-check and extend-check
        to improve performance for particular usage patterns. Empirically the extend
        check fails more often and takes care of about 9% of cases. The shrink test
        is more likely to succeed (provided it's a real case in an application) and
        is therefore placed first. This succeeds in about 40% of cases.
    */


    /*
        Can we shrink?
    */
    Uq_ptr p;
    p.addr = (size_t)ptr - sizeof(q_ptr);
    size_t sz = *p.len;
    if(sz >= len + sizeof(q_ptr)){
        //We can shrink current allocation
        *p.len = len;
        p.addr += len + sizeof(q_ptr);
        if(p.addr <= __q_heap_end.addr){
            *p.len = (sz - len - sizeof(q_ptr)) | Q_FREEMASK;
        }
        #ifdef _Q_DEBUG_
        way = 4;
        way1 ++;
        #endif
        return ptr;
    }

    /*
        Can we extend in place?
    */
    Uq_ptr q = p;
    q.addr += sz + sizeof(q_ptr);
    if((q.addr <= __q_heap_end.addr) && (*q.len & Q_FREEMASK)){ // Is the next block free?
        size_t l = (*q.len & ~Q_FREEMASK) + sz + sizeof(q_ptr);
        if(l >= len){                 // If so, can we extend?
            //q_listalloc();
            //exit(0);
            q.addr = p.addr + len + sizeof(q_ptr);
            if(q.addr <= __q_heap_end.addr){
                size_t nl = (l - len - sizeof(q_ptr));
                if(nl <= Q_MAXLEN){
                    *p.len = len;
                    *q.len = nl | Q_FREEMASK;
                    #ifdef _Q_DEBUG_
                    way = 7;
                    way2 ++;
                    #endif // _Q_DEBUG_
                    return ptr;
                }
            }
        }
    }

    /*
        Okay, we need to do this in the trivial way.
    */
    void *newptr = q_malloc(len);
    if( newptr ){
        if(len > sz){
            len = sz;
        }
        memmove(newptr, ptr, len);
        __q_free(ptr);
        #ifdef _Q_DEBUG_
        way =5;
        way3 ++;
        #endif // _Q_DEBUG_
        return newptr;
    }
    #ifdef _Q_DEBUG_
    way = 6;
    #endif
    return 0;
}
#endif // Q_REALLOC_TRIVIAL
/**
    Realloc maximizing chance for successful allocation by copying old data to a buffer
    on stack, then freeing and then allocating new space. Finishing with copying buffer
    from stack back to the new area.
    Method need [len] bytes extra on stack and uses two memmoves.
    Note: There is a possibility of a silent fail when q_realloc fails to extend the
    allocation and re-allocs the


*/

#ifdef Q_REALLOC_STACK
void *q_realloc(void *ptr, size_t len){
    //    size_t n;
    size_t orgptr = (size_t)ptr;
    if(ptr == 0){       // If ptr is null, work just like q_malloc
        ptr = q_malloc(len);
        way = 1;
        return ptr;
    }

    if( ((len > Q_MAXLEN) || ((size_t)ptr < __q_heap_start.addr) || ((size_t)ptr > __q_heap_end.addr)) ){ // Is it a valid pointer to heap?
        way = 2;
        return 0;
    }

    if(len == __q_size_internal(ptr)){   // Len is same, just return it
        way = 3;
        return ptr;
    }

    void *newptr = ptr;


    size_t oz = __q_size_internal(ptr);
    if(oz >= len + sizeof(q_ptr)){
        //We can shrink current allocation
        Uq_ptr p;
        p.addr = (size_t)ptr - sizeof(q_ptr);
        *p.len = len;
        p.addr += len + sizeof(q_ptr);
        if(p.addr <= __q_heap_end.addr){
            *p.len = (oz - len - sizeof(q_ptr)) | Q_FREEMASK;
        }
        way1 ++;
        way = 4;
        if(__q_size_internal(ptr) != len){
            printf("\nWhat the hell? %lu / %lu", oz, __q_size_internal(ptr));
        }
        if(orgptr != (size_t)ptr){
            printf("\nWhat the heell 2??");
            exit(0);
        }
        return ptr;
    }
    // If we are here, the new length must be > original length - sizeof(q_ptr), but not equal to original length

    size_t sz = len;
    if(len > oz){
        sz = oz;
    }

    uint8_t buf[oz];                // Hold whole old buffer on stack
    memmove(buf, ptr, oz);          // Hold old data in stack buffer
    __q_free(ptr);                  // Free space to make it easier for newmalloc()
    newptr = q_malloc(len);
    if( newptr ){                   // Success? Then copy data to new location
        memmove(newptr, buf, sz);
        way2 ++;
        way = 5;
        return newptr;
    }else{
        ptr = q_malloc(oz);      // Okay, so we must at least be able to allocate original length right?
        way3 ++;

        if( ptr ){
            //__q_unfree_internal(ptr);   // No? Then restore
            memmove(ptr, buf, oz);
            way = 6;
            return ptr;                 // We failed. We have to return the new pointer, the fail is silent
        }else{
            way = 7;
            printf("what the hell???");
            exit(0);                        //
        }
    }
    way = 8;
    return 0;
}
#endif // Q_REALLOC_STACK

#define Q_MAX_STACK_ALLOC   64
/**
    Realloc check the new len, and if it's short it puts the old area on stack, freeing old area,
    then try to alloc new length, copying data from stack to new area. This maximizes then chances
    for realloc to succeed.
    If new length is larger, try to alloc new length and then moving data.
    Note: if realloc fails the ptr may have moved, even if still valid.
    Do not use copies of this pointer after q_realloc and expecting them to point
    to the buffer.

*/
#ifdef Q_REALLOC_TRYSTACK
void *q_realloc(void *ptr, size_t len){
    //    size_t n;
    if(ptr == 0){       // If ptr is null, work just like q_malloc
        return q_malloc(len);
        way = 1;
    }

    if( ((len > Q_MAXLEN) || ((size_t)ptr < __q_heap_start.addr) || ((size_t)ptr > __q_heap_end.addr)) ){ // Is it a valid pointer to heap?
        way = 2;
        return 0;
    }

    if(len == __q_size_internal(ptr)){   // Len is same, just return it
        way = 3;
        return ptr;
    }

    size_t oz = __q_size_internal(ptr);
    if(oz >= len + sizeof(q_ptr)){
        //We can shrink current allocation
        Uq_ptr p;
        p.addr = (size_t)ptr - sizeof(q_ptr);
        *p.len = len;
        p.addr += len + sizeof(q_ptr);
        if(p.addr <= __q_heap_end.addr){
            *p.len = (oz - len - sizeof(q_ptr)) | Q_FREEMASK;
        }
        way1 ++;
        way = 4;
        return ptr;
    }


    if(len < Q_MAX_STACK_ALLOC){
        void *newptr = ptr;
        way2 ++;
        size_t sz = len;
        //size_t oz = __q_size_internal(ptr);
        if(len > oz){
            sz = oz;
        }

        uint8_t buf[sz];
        memmove(buf, ptr, sz);          // Hold old data in stack buffer
        __q_free(ptr);                  // Free space to make it easier for newmalloc()
        newptr = q_malloc(len);
        if( newptr ){                   // Success? Then copy data to new location
            memmove(newptr, buf, sz);
            way = 5;
            return newptr;
        }else{
            ptr = q_malloc(oz);      // Okay, so we must at least be able to allocate original length right?
            if( ptr ){
                //__q_unfree_internal(ptr);   // No? Then restore
                memmove(ptr, buf, oz);
                way = 6;
                return ptr;         // We must return the pointer since chunk place has moved. This is a silent fail.
            }else{
                printf("what the hell???");
                way = 7;
                exit(0);
            }
        }
    }else{
        // Okay, so the asked length is larger than Q_MAX_STACK_ALLOC
        way3 ++;
        void *newptr = q_malloc(len);
        if( newptr ){
            //size_t sz = __q_size_internal(ptr);
            if (oz > len){
                oz = len;
            }
            //printf(("yay"));
            memcpy(newptr, ptr, oz);
            __q_free(ptr);
            way = 8;
            return newptr;
        }
    }
    way = 9;
    return 0;
}
#endif // Q_REALLOC_TRYSTACK


void *q_calloc(size_t __nele, size_t __size) {
    size_t size = __nele * __size;
    //size_t n;
    void *p = q_malloc(size);
    if(p){
        #ifdef _Q_DEBUG_
        way = 22;
        #endif
        memset(p, 0, size);
        return p;
    }
    return 0;
}




#ifdef _Q_DEBUG_

void getways(size_t *w1, size_t *w2, size_t *w3, size_t *w4){
    *w1 = way1;
    *w2 = way2;
    *w3 = way3;
    *w4 = way4;
}

size_t reladr(size_t addr){
    return addr - __q_heap_start.addr;
}

void ptrcheck(size_t ptr, long unsigned int dbg){
    if((ptr > __q_heap_end.addr) || (ptr < __q_heap_start.addr)){
        printf("\nPointer err in %lu. Pointer is: %lu  Heap is %lu ... %lu\n", dbg, reladr(ptr), reladr(__q_heap_start.addr), reladr(__q_heap_end.addr));
        q_listalloc();
        exit(0);
    }
}

void q_listalloc(void) {
    Uq_ptr ptr = __q_heap_start;
    size_t total = 0;
    Uq_ptr last = ptr;

    size_t lastl = 0;
    while(ptr.addr < __q_heap_end.addr){
        q_ptr l = *ptr.len;
        if(l & Q_FREEMASK){
            printf("\n*BLOCK FREE at %05lx of len %lu", reladr(ptr.addr), l & ~Q_FREEMASK);
            total += (l & ~Q_FREEMASK)+ sizeof(q_ptr);
        }else{
            #define Q_LOCAL_BUFSIZE 256
            char buf[Q_LOCAL_BUFSIZE];
            size_t ln;
            if(l > Q_LOCAL_BUFSIZE){
                ln = Q_LOCAL_BUFSIZE - 1;
            }else{
                ln = l;
            }
            memmove(buf, (q_ptr*)(ptr.addr + sizeof(q_ptr)), ln);
            buf[ln] = 0;
            printf("\n Used block at %05lx of len %lu: '%s'", reladr(ptr.addr), (size_t)l, buf);
            if(!l){
              memmove(buf, (q_ptr*)(ptr.addr - sizeof(q_ptr)), 20);
              printf(" <--- *** NULL BLOCK ***");
              for(uint16_t j = 0; j < 20 ; j++){
                if((buf[j] > 30) && (buf[j] < 127)){
                    printf("%c", buf[j]);

                }else{
                    printf(" ");
                }
              }
            }

            total += l + sizeof(q_ptr);

        }

        last = ptr;
        lastl = last.addr + (l & ~Q_FREEMASK) + sizeof(q_ptr) - 1;
        ptr.addr = ptr.addr + (l & ~Q_FREEMASK) + sizeof(q_ptr);

    }
    printf("\nTotal: %lu  Free: %lu Last ptr: %05lx-->%05lx", total,q_get_freemem(), reladr(last.addr), reladr(lastl));
}

size_t q_countalloc(void) {
    Uq_ptr ptr = __q_heap_start;
    size_t total = 0;
    size_t ret = 1000000000;
    while(ptr.addr < __q_heap_end.addr){
        q_ptr l = *ptr.len;
        if(l & Q_FREEMASK){
            total += (l & ~Q_FREEMASK)+ sizeof(q_ptr);
        }else{
            if((size_t)l < ret) ret = l;
            total += l + sizeof(q_ptr);

        }
        ptr.addr = ptr.addr + (l & ~Q_FREEMASK) + sizeof(q_ptr);

    }

    return ret;
}


size_t q_countzeroalloc(void) {
    Uq_ptr ptr = __q_heap_start;
    size_t ret = 0;
    while(ptr.addr < __q_heap_end.addr){
        q_ptr l = *ptr.len;
        if(l == 0){     // Block is not free and of zero length
            ret ++;
        }
        ptr.addr = ptr.addr + (l & ~Q_FREEMASK) + sizeof(q_ptr);
    }
    return ret;
}


#endif



/**
void *q_realloc(void *ptr, size_t len){
    //    size_t n;
    if(ptr == 0){       // If ptr is null, work just like q_malloc
        way = 1;
        return q_malloc(len);
    }

    if( ((len > Q_MAXLEN) || ((size_t)ptr < __q_heap_start.addr) || ((size_t)ptr > __q_heap_end.addr)) ){ // Is it a valid pointer to heap?
        way = 2;
        return 0;
    }

    if(len == __q_size_internal(ptr)){   // Len is same, just return it
        way = 3;
        return ptr;
    }

    size_t sz = __q_size_internal(ptr);
    if(sz >= len + sizeof(q_ptr)){
        //We can shrink current allocation
        Uq_ptr p;
        p.addr = (size_t)ptr - sizeof(q_ptr);
        *p.len = len;
        p.addr += len + sizeof(q_ptr);
        if(p.addr <= __q_heap_end.addr){
            *p.len = (sz - len - sizeof(q_ptr)) | Q_FREEMASK;
        }
        way = 4;
        return ptr;
    }

    void *newptr = q_malloc(len);
    if( newptr ){
        if(len > sz){
            len = sz;
        }
        memmove(newptr, ptr, len);
        __q_free(ptr);
        way =5;
        return newptr;
    }
    way = 6;
    return 0;
}
*/
