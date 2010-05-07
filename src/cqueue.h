
#ifndef __CQUEUE_H__
#define __CQUEUE_H__ 1

typedef struct CQueue_ {
    unsigned char *cqueue;
    unsigned char *cqueue_end;    
    unsigned char *first_element;
    unsigned char *last_element;
    size_t cqueue_size;
    size_t element_size;
} CQueue;

int init_cqueue(CQueue * const cqueue, const size_t nb_elements,
                const size_t element_size);

void free_cqueue(CQueue * const cqueue);
int push_cqueue(CQueue * const cqueue, const void * const pnt);
void * shift_cqueue(CQueue * const cqueue);

#endif
