
#include "common.h"
#include "stack.h"

int init_pnt_stack(PntStack * const pnt_stack,
                   const size_t initial_nb_elements,
                   const size_t element_size)
{
    if (SIZE_MAX / initial_nb_elements < element_size) {
        return -1;
    }
    pnt_stack->element_size = element_size;
    pnt_stack->stack_size =
        (initial_nb_elements * element_size + STACK_CHUNK_SIZE_1) &
        ~ STACK_CHUNK_SIZE_1;
    pnt_stack->low_depth = pnt_stack->stack_size / (size_t) 2U;
    pnt_stack->depth = (size_t) 0U;
    pnt_stack->low_depth_unreached_count = 0U;
    if ((pnt_stack->stack = malloc(pnt_stack->stack_size)) == NULL) {
        return -1;
    }
    return 0;
}

void free_pnt_stack(PntStack * const pnt_stack)
{
    if (pnt_stack == NULL) {
        return;
    }
    free(pnt_stack->stack);
    pnt_stack->stack = NULL;
    free(pnt_stack);
}

PntStack *new_pnt_stack(size_t initial_nb_elements,
                        const size_t element_size)
{
    PntStack *pnt_stack;
    
    if (initial_nb_elements <= (size_t) 0U) {
        initial_nb_elements = STACK_CHUNK_SIZE / element_size;
        if (initial_nb_elements <= 0U) {
            return NULL;
        }
    }
    pnt_stack = malloc(sizeof *pnt_stack);
    init_pnt_stack(pnt_stack, initial_nb_elements, element_size);
    
    return pnt_stack;
}

int push_pnt_stack(PntStack * const pnt_stack, const void * const pnt)
{
    size_t new_stack_size;
    unsigned char *new_stack;
    
    if (pnt_stack->depth < pnt_stack->stack_size) {
        memcpy(pnt_stack->stack + pnt_stack->depth, pnt,
               pnt_stack->element_size);
        pnt_stack->depth += pnt_stack->element_size;
        assert(pnt_stack->depth <= pnt_stack->stack_size);
        if (pnt_stack->stack_size <= STACK_CHUNK_SIZE ||
            pnt_stack->stack_size <= pnt_stack->element_size) {
            return 0;
        }
        if (pnt_stack->depth > pnt_stack->low_depth) {
            pnt_stack->low_depth_unreached_count = 0U;
            return 0;
        }
        if (++(pnt_stack->low_depth_unreached_count) <
            STACK_SHRINK_AFTER_LOW_DEPTH_COUNT) {
            return 0;
        }
        pnt_stack->stack_size = pnt_stack->low_depth;
        pnt_stack->low_depth /= (size_t) 2U;
        pnt_stack->low_depth_unreached_count = 0U;
        new_stack = realloc(pnt_stack->stack, pnt_stack->stack_size);
        if (new_stack != NULL) {
            pnt_stack->stack = new_stack;
        }        
        return 0;
    }
    new_stack_size = pnt_stack->stack_size * (size_t) 2U;
    if (new_stack_size <= pnt_stack->stack_size) {
        return -1;
    }    
    new_stack = realloc(pnt_stack->stack, new_stack_size);
    if (new_stack == NULL) {
        return -1;
    }
    pnt_stack->low_depth = pnt_stack->stack_size;
    pnt_stack->stack_size = new_stack_size;
    pnt_stack->low_depth_unreached_count = 0U;
    pnt_stack->stack = new_stack;
    memcpy(pnt_stack->stack + pnt_stack->depth, pnt, pnt_stack->element_size);
    pnt_stack->depth += pnt_stack->element_size;
    
    return 0;
}

void *pop_pnt_stack(PntStack * const pnt_stack)
{
    if (pnt_stack->depth <= (size_t) 0U) {
        return NULL;
    }
    assert(pnt_stack->depth >= pnt_stack->element_size);
    pnt_stack->depth -= pnt_stack->element_size;
    
    return pnt_stack->stack + pnt_stack->depth;
}

int pnt_stack_foreach(PntStack * const pnt_stack, PntStackForeachCB cb,
                      void * const context)
{
    size_t depth = (size_t) 0U;
    
    while (depth < pnt_stack->depth) {
        if (cb(context, pnt_stack->stack + depth) != 0) {
            break;
        }
        depth += pnt_stack->element_size;
    }
    return 0;
}

_Bool pnt_stack_exists(PntStack * const pnt_stack, const void * const pnt)
{
    size_t depth = (size_t) 0U;
    const size_t element_size = pnt_stack->element_size;
    
    while (depth < pnt_stack->depth) {
        if (memcmp(pnt, pnt_stack->stack + depth, element_size) == 0) {
            return 1;
        }
        depth += pnt_stack->element_size;
    }
    return 0;
}

int init_pnt_stack_iterator(PntStackIterator * const pnt_stack_iterator,
                            PntStack * const pnt_stack)
{
    pnt_stack_iterator->pnt_stack = pnt_stack;
    pnt_stack_iterator->depth = (size_t) 0U;
    
    return 0;
}

int pnt_stack_iterator_rewind(PntStackIterator * const pnt_stack_iterator)
{
    pnt_stack_iterator->depth = (size_t) 0U;
    
    return 1;
}

void *pnt_stack_iterator_next(PntStackIterator * const pnt_stack_iterator)
{
    size_t depth = pnt_stack_iterator->depth;
    PntStack * const pnt_stack = pnt_stack_iterator->pnt_stack;
    
    if (depth >= pnt_stack->depth) {
        pnt_stack_iterator->depth = (size_t) 0U;
        return NULL;
    }        
    void * const item = pnt_stack->stack + depth;    
    depth += pnt_stack->element_size;
    pnt_stack_iterator->depth = depth;
    
    return item;
}

void *pnt_stack_cyterator_next(PntStackIterator * const pnt_stack_iterator)
{
    size_t depth = pnt_stack_iterator->depth;
    PntStack * const pnt_stack = pnt_stack_iterator->pnt_stack;
    void * const item = pnt_stack->stack + depth;
    
    depth += pnt_stack->element_size;
    if (depth >= pnt_stack->depth) {
        depth = (size_t) 0U;
    }
    pnt_stack_iterator->depth = depth;
    
    return item;
}
