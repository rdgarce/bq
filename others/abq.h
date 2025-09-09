#ifndef ABQ_H
#define ABQ_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <assert.h>

typedef struct
{
    uint8_t *data;
    size_t head;
    size_t tail;
    size_t nelem;
    size_t size;
    pthread_mutex_t lock;
} abq;

static void abq_queue_init(abq *q, uint8_t *buffer, size_t size)
{
    q->data = buffer;
    q->size = size;
    q->head = 0;
    q->tail = 0;
    q->nelem = 0;
    int r = pthread_mutex_init(&q->lock, NULL);
    assert(!r);
}

static void abq_queue_free(abq *q)
{
    int r = pthread_mutex_destroy(&q->lock);
    assert(!r);
}

static void *abq_queue_get_push_buf(abq *q, size_t *len)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    if (q->head < q->tail || q->nelem == 0)
        *len = q->size - q->tail;
    else // q->head >= q->tail
        *len = q->head - q->tail;

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);

    return q->data + q->tail;
}

static void abq_queue_commit_push(abq *q, size_t len)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    q->tail = (q->tail + len) % q->size;
    q->nelem += len;

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);
}

static void *abq_queue_get_pop_buf(abq *q, size_t *len)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    if (q->head < q->tail || q->nelem == 0)
        *len = q->tail - q->head;
    else // q->head >= q->tail
        *len = q->size - q->head;

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);
        
    return q->data + q->head;
}

static void abq_queue_commit_pop(abq *q, size_t len)
{
    int r = pthread_mutex_lock(&q->lock);
    assert(!r);

    q->head = (q->head + len) % q->size;
    q->nelem -= len;

    r = pthread_mutex_unlock(&q->lock);
    assert(!r);
}

#endif