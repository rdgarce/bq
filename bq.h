/*
 * MIT License
 * 
 * Copyright (c) 2025 Raffaele del Gaudio, https://delgaudio.me
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef BQ_H
#define BQ_H

/* An efficient ring buffer implementation.
 * This is suitable for a SPSC scenario with at most two concurrent
 * executing threads. Some notable facts:
 * 1: To correctly represent the ring state you only need two variables:
 *      head + tail or head + num_element. You don't strictly need the
 *      three of them. In this implementation, others are added, but
 *      solely for a performance increase.
 * 2: The provided API allows to skip the unnecessary user-to-user copy
 *      when producing and consuming bytes.
 * 2: By using head + tail insted of head + num_element the ring can be
 *      lock-free because even if there is a data race on head and tail
 *      between producer and consumer, the fact that each one of them
 *      only updates one and only one variable and that a stale value
 *      corresponds anyway to a correct ring state, there are no undefined
 *      behaviours. The only needed thing is that head and tail are
 *      updated with a release consistency and read with an acquire
 *      consistency to be sure that the buffer memory is updated before
 *      the state variable.
 *      If head + num_elemen is chosen, then you have to atomically
 *      modify both of them in the consumer after a pop or you would
 *      have an incorrect state. This leads to the need for a
 *      lock or to store both variable in a single word accessible
 *      atomically, but this seems to me too much of a trick considering
 *      the other option.
 * 3: By restricting the queue length to be a power of 2 and storing the values
 *      of head and tail without doing the modulo, you solve the problem of
 *      not knowing whether the ring is empty of full when head == tail.
 *      Now, head == tail always means that the ring is empty and
 *      (tail - head) == capacity always means that it is full. Even when tail
 *      wraps around SIZE_MAX, the implicit (mod SIZE_MAX+1) on all operations
 *      allows the result to be correct. NOTE: When storing head and tail without
 *      the modulo, the queue length MUST be a power of 2, otherwise the implicit
 *      (mod SIZE_MAX+1) on all operations will lead to incorrect states.
 *      EXAMPLE:
 *      Suppose the queue is size 3 and head is SIZE_MAX, about to tick over to zero:
 *      To pop the next element you access data[head % 3] == data[0], because
 *      SIZE_MAX % 3 == 0. After the pop the element, you update head as follows:
 *      head = head + 1 == 0, because SIZE_MAX + 1 == 0.
 *      We've incremented head but the effective value is still zero.
 * 4: By having the length equal to a power of two, all modulo operations
 *      become bitwise operations.
 * 5: By having the length equal to a power of two, you can use bitwise
 *      operations to have a complete branchless implementation.
 */

#include <stddef.h>
#include <stdint.h>

typedef struct
{
    size_t head, tail;
    size_t mask;
    unsigned char cap_lg2;
    char *data;
} bq;

/* Returns a byte queue given the buffer [buf] of size [len].
 * It's suggested that len is a power of two because the
 * implementation allocates the queue in the biggest and fully
 * contained slice of [buf] measuring a power of two. */
static bq bq_make(char *buf, size_t len)
{
    if (!buf || !len) return (bq){0};
    
    // Calculates the position of the msb setted in len
    unsigned char msb = 0;
    if (len >> 32) { len >>= 32; msb += 32; }
    if (len >> 16) { len >>= 16; msb += 16; }
    if (len >> 8)  { len >>= 8;  msb += 8;  }
    if (len >> 4)  { len >>= 4;  msb += 4;  }
    if (len >> 2)  { len >>= 2;  msb += 2;  }
    if (len >> 1)  {             msb += 1;  }
    
    return (bq){.head = 0, .tail = 0, .mask = (1UL << msb) - 1,
        .cap_lg2 = msb, .data = buf};
}

/* Given the byte queue [q], returns a pointer to the buffer of
 * poppable bytes and sets [*len] to the len of the buffer */
static void *bq_popbuf(bq *q, size_t *len)
{
    // This private copy of tail is essential to have a coherent
    // value throughout the function, regardless of the consumer's
    // actions.
    // The ACQUIRE semantic is required because, otherwise, reads to
    // q->data[q->head] can be reordered before the q->tail read.
    // If the read is also reordered before the writes of the same
    // bytes by the producer in the memory total order, the consumer
    // will read bytes not yet produced.
    size_t tail = __atomic_load_n(&q->tail, __ATOMIC_ACQUIRE);
    // The cond variable is 0 iff tail is in the same block of
    // (q->mask + 1) bytes, otherwise is:
    // -- 1, when tail is in the next block and has not wrapped around SIZE_MAX,
    // -- A big odd negative number, when tail has wrapped around SIZE_MAX.
    // With the final bitwise AND, we reduce the possible results to (0, 1).
    // We use this variable to subtract from the final number conditionally.
    size_t cond = ((tail >> q->cap_lg2) - (q->head >> q->cap_lg2)) & 0x1;
    *len = tail - q->head - (tail & q->mask) * cond;

    return q->data + (q->head & q->mask);
}

/* Given the byte queue [q], pops [count] bytes from it.
 * This function should be called after a bq_popbuf or a
 * bq_nelem to pop a certain number of bytes.
 * [count] MUST be in any case less than or equal to the
 * total number of bytes in available the queue */
static void bq_pop(bq *q, size_t count)
{
    // Atomic store with release consistency because we need to be
    // sure that head is updated after the producer actually copied
    // the bytes outside the queue
    __atomic_store_n(&q->head, q->head + count, __ATOMIC_RELEASE);
}

/* Given the byte queue [q], returns a pointer to the buffer of
 * pushable bytes and sets [*len] to the len of the buffer */
static void *bq_pushbuf(bq *q, size_t *len)
{
    // This private copy of head is essential to have a coherent
    // value throughout the function, regardless of the producer's
    // actions.
    // The ACQUIRE semantic is required because, otherwise, writes to
    // q->data[q->tail] can be reordered before the q->head read.
    // If the write is also reordered before the read of the same
    // bytes by the consumer in the memory total order, the producer
    // will overwrite still unconsumed bytes.
    size_t head = __atomic_load_n(&q->head, __ATOMIC_ACQUIRE);
    // The cond variable is 0 iff tail is in the same block of
    // (q->mask + 1) bytes, otherwise is:
    // -- 1, when tail is in the next block and has not wrapped around SIZE_MAX,
    // -- A big odd negative number, when tail has wrapped around SIZE_MAX.
    // With the final bitwise AND, we reduce the possible results to (0, 1).
    // We use this variable to subtract from the final number conditionally.
    size_t cond = ((q->tail >> q->cap_lg2) - (head >> q->cap_lg2)) & 0x1;
    *len = q->mask + 1 - (q->tail - head) - (head & q->mask) * (1 - cond);

    return q->data + (q->tail & q->mask);
}

/* Given the byte queue [q], push [count] bytes to it.
 * This function MUST be called after a bq_pushbuf to
 * commit the push a certain number of bytes. [count]
 * MUST be in any case less than or equal to the
 * len value returned by the last bq_pushbuf */
static void bq_push(bq *q, size_t count)
{
    // Atomic store with release consistency because we need to be
    // sure that tail is updated after the consumer actually copied
    // the bytes in the queue
    __atomic_store_n(&q->tail, q->tail + count, __ATOMIC_RELEASE);
}

#endif