#ifndef BBQ_H
#define BBQ_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>
#include <stdbool.h>

typedef struct
{
    uint8_t *data;
    size_t head;
    size_t tail;
    size_t nelem;
    size_t size;
    pthread_mutex_t lock;
} bbq;

static void bbq_queue_init(bbq *q, uint8_t *buffer, size_t size)
{
    q->data = buffer;
    q->size = size;
    q->head = 0;
    q->tail = 0;
    q->nelem = 0;
    int r = pthread_mutex_init(&q->lock, NULL);
    assert(!r);
}

static void bbq_queue_free(bbq *q)
{
    int r = pthread_mutex_destroy(&q->lock);
    assert(!r);
}

static bool bbq_queue_push(bbq *q, uint8_t byte)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);
    
    bool ok = false;
    if (q->nelem < q->size)
    {
        q->data[q->tail] = byte;
        q->tail = (q->tail + 1) % q->size;
        q->nelem++;
        ok = true;
    }

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);

    return ok;
}

static bool bbq_queue_pop(bbq *q, uint8_t *byte)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    bool ok = false;
    if (q->nelem > 0)
    {
        *byte = q->data[q->head];
        q->head = (q->head + 1) % q->size;
        q->nelem--;
        ok = true;
    }

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);

    return ok;
}

#endif