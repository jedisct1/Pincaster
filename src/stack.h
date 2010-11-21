
#ifndef __STACK_H__
#define __STACK_H__ 1

#ifndef STACK_CHUNK_SIZE
# define STACK_CHUNK_SIZE   ((size_t) 4096U)
#endif
#define STACK_CHUNK_SIZE_1  (STACK_CHUNK_SIZE - (size_t) 1U)
#ifndef STACK_SHRINK_AFTER_LOW_DEPTH_COUNT
# define STACK_SHRINK_AFTER_LOW_DEPTH_COUNT 100U
#endif

typedef struct PntStack_ {
    unsigned char *stack;
    size_t stack_size;
    size_t element_size;
    size_t depth;
    size_t low_depth;
    unsigned int low_depth_unreached_count;
} PntStack;

typedef struct PntStackIterator_ {
    PntStack * pnt_stack;
    size_t depth;
} PntStackIterator;

int init_pnt_stack(PntStack * const pnt_stack,
                   const size_t initial_nb_elements,
                   const size_t element_size);

void free_pnt_stack(PntStack * const pnt_stack);

PntStack *new_pnt_stack(size_t initial_nb_elements,
                        const size_t element_size);

int push_pnt_stack(PntStack * const pnt_stack, const void * const pnt);

void * pop_pnt_stack(PntStack * const pnt_stack);

typedef int (*PntStackForeachCB)(void *context, void *pnt);

int pnt_stack_foreach(PntStack * const pnt_stack, PntStackForeachCB cb,
                      void * const context);

_Bool pnt_stack_exists(PntStack * const pnt_stack, const void * const pnt);

int init_pnt_stack_iterator(PntStackIterator * const pnt_stack_iterator,
                            PntStack * const pnt_stack);

int pnt_stack_iterator_rewind(PntStackIterator * const pnt_stack_iterator);
void *pnt_stack_iterator_next(PntStackIterator * const pnt_stack_iterator);
void *pnt_stack_cyterator_next(PntStackIterator * const pnt_stack_iterator);

#endif
