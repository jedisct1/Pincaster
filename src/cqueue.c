
#include "common.h"
#include "cqueue.h"

int init_cqueue(CQueue * const cqueue, const size_t nb_elements,
                const size_t element_size)
{
    size_t cqueue_size;
    
    if (nb_elements < (size_t) 2U || SIZE_MAX / nb_elements < element_size) {
        return -1;
    }
    cqueue->element_size = element_size;
    cqueue_size = nb_elements * element_size;
    if ((cqueue->cqueue = malloc(cqueue_size)) == NULL) {
        return -1;
    }
    cqueue->cqueue_end = cqueue->cqueue + (cqueue_size - element_size);
    cqueue->first_element = cqueue->last_element = NULL;
    
    return 0;
}

void free_cqueue(CQueue * const cqueue)
{
    if (cqueue == NULL) {
        return;
    }
    free(cqueue->cqueue);
    cqueue->cqueue = cqueue->cqueue_end = NULL;
    cqueue->first_element = cqueue->last_element = NULL;
    free(cqueue);
}

int push_cqueue(CQueue * const cqueue, const void * const pnt)
{
    unsigned char *new_element;

    if (cqueue->last_element == NULL) {
        assert(cqueue->first_element == NULL);
        new_element = cqueue->first_element = cqueue->cqueue;
    } else {
        if (cqueue->last_element != cqueue->cqueue_end) {
            new_element = cqueue->last_element + cqueue->element_size;
        } else {
            new_element = cqueue->cqueue;
        }
        if (new_element == cqueue->first_element) {
            return -1;
        }
    }
    memcpy(new_element, pnt, cqueue->element_size);
    cqueue->last_element = new_element;
    
    return 0;
}

void * shift_cqueue(CQueue * const cqueue)
{
    unsigned char *next_element;
    unsigned char *element;
    
    if (cqueue->first_element == NULL) {
        return NULL;
    }
    if (cqueue->first_element == cqueue->last_element) {    
        cqueue->last_element = next_element = NULL;
    } else {
        if (cqueue->first_element != cqueue->cqueue_end) {
            next_element = cqueue->first_element + cqueue->element_size;
        } else {
            next_element = cqueue->cqueue;
        }
    }
    element = cqueue->first_element;    
    cqueue->first_element = next_element;
    
    return element;
}
