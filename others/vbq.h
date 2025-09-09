#ifndef VBQ_H
#define VBQ_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

typedef struct
{
    uint8_t *data;
    size_t head;
    size_t tail;
    size_t nelem;
    size_t size;
    pthread_mutex_t lock;
} vbq;

static void vbq_queue_init(vbq *q, uint8_t *buffer, size_t size)
{
    q->data = buffer;
    q->size = size;
    q->head = 0;
    q->tail = 0;
    q->nelem = 0;
    int r = pthread_mutex_init(&q->lock, NULL);
    assert(!r);
}

static void vbq_queue_free(vbq *q)
{
    int r = pthread_mutex_destroy(&q->lock);
    assert(!r);
}

static size_t vbq_queue_push_vector(vbq *q, const uint8_t *bytes, size_t count)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    size_t tailcopy_size = 0;
    size_t total = 0;

    if (q->head < q->tail || q->nelem == 0)
    {
        tailcopy_size = MIN(q->size - q->tail, count);
        memcpy(q->data + q->tail, bytes, tailcopy_size);
        q->tail = (q->tail + tailcopy_size) % q->size;
        total += tailcopy_size;
    }

    size_t headcopy_size = MIN(q->head - q->tail, count - tailcopy_size);
    if (headcopy_size > 0)
    {
        memcpy(q->data + q->tail, bytes + tailcopy_size, headcopy_size);
        q->tail += headcopy_size;
        total += headcopy_size;
    }
    
    q->nelem += total;    

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);

    return total;
}

static size_t vbq_queue_pop_vector(vbq *q, uint8_t *bytes, size_t count)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    size_t total;

    if (q->nelem == 0)
    {
        r = pthread_mutex_unlock(&q->lock);
        assert(!r);
        return 0;
    }

    if (q->head < q->tail)
    {
        size_t to_pop = MIN(q->tail - q->head, count);
        memcpy(bytes, q->data + q->head, to_pop);
        q->head = (q->head + to_pop) % q->size;
        q->nelem -= to_pop;
        total = to_pop;
    }
    else
    {
        // q->head >= q->tail
        size_t frst_pop = MIN(q->size - q->head, count);
        memcpy(bytes, q->data + q->head, frst_pop);
        q->head = (q->head + frst_pop) % q->size;
        q->nelem -= frst_pop;
        
        size_t scnd_pop = MIN(q->tail - q->head, count - frst_pop);
        if (scnd_pop > 0)
        {
            memcpy(bytes + frst_pop, q->data + q->head, scnd_pop);
            q->head = (q->head + scnd_pop) % q->size;
            q->nelem -= scnd_pop;
        }

        total = frst_pop + scnd_pop;
    }

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);

    return total;
}

#endif