#ifndef LFQ_H
#define LFQ_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    uint8_t *data;
    size_t head;
    size_t tail;
    size_t size;
} lfq;

static void lfq_queue_init(lfq *q, uint8_t *buffer, size_t size)
{
    q->data = buffer;
    q->size = size;
    q->head = 0;
    q->tail = 0;
}

static void *lfq_queue_get_push_buf(lfq *q, size_t *len)
{
    // This private copy of head is essential to have a coherent
    // value throughout the function, regardless of the consumer's
    // actions.
    size_t head = q->head;
    size_t head_n_wrap = head / q->size;
    size_t tail_n_wrap = q->tail / q->size;

    if (head_n_wrap == tail_n_wrap)
        // head and tail are in the same block of
        // q->size bytes
        *len = q->size - (q->tail % q->size);
    else
        *len = q->size - (q->tail - head);

    return q->data + (q->tail % q->size);
}

static void lfq_queue_commit_push(lfq *q, size_t len)
{
    __atomic_store_n(&q->tail, q->tail + len, __ATOMIC_RELEASE);
}

static void *lfq_queue_get_pop_buf(lfq *q, size_t *len)
{
    // This private copy of tail is essential to have a coherent
    // value throughout the function, regardless of the consumer's
    // actions.
    size_t tail = q->tail;
    size_t head_n_wrap = q->head / q->size;
    size_t tail_n_wrap = tail / q->size;

    if (head_n_wrap == tail_n_wrap)
        // head and tail are in the same block of
        // q->size bytes
        *len = tail - q->head;
    else // q->head >= q->tail
        *len = tail - q->head - (tail % q->size);

    return q->data + (q->head % q->size);
}

static void lfq_queue_commit_pop(lfq *q, size_t len)
{
    __atomic_store_n(&q->head, q->head + len, __ATOMIC_RELEASE);
}

#endif