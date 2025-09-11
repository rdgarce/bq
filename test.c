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

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include "others/bbq.h"
#include "others/vbq.h"
#include "others/abq.h"
#include "others/lfq.h"

#include "bq.h"
#include "profiler.h"

#define BYTES_TO_PRODUCE    1024*1024*1024ull
#define QUEUE_SIZE          1024*1024ull
#define MAX_BYTES_PER_OP    1024ull
#define MAX_SLEEP_USEC      50

static void *producer_thread(void *arg);
static void *consumer_thread(void *arg);

static bbq bbq_queue;
static vbq vbq_queue;
static abq abq_queue;
static lfq lfq_queue;
static bq bq_queue;

static bool prod_finished;

int main()
{
    srand(time(NULL));
    prod_finished = false;

    bbq_queue_init(&bbq_queue, malloc(QUEUE_SIZE), QUEUE_SIZE);
    vbq_queue_init(&vbq_queue, malloc(QUEUE_SIZE), QUEUE_SIZE);
    abq_queue_init(&abq_queue, malloc(QUEUE_SIZE), QUEUE_SIZE);
    lfq_queue_init(&lfq_queue, malloc(QUEUE_SIZE), QUEUE_SIZE);
    bq_queue = bq_make(malloc(QUEUE_SIZE), QUEUE_SIZE);

    assert(bbq_queue.data && vbq_queue.data &&
        abq_queue.data && lfq_queue.data && bq_queue.data);

    printf("Running test on moving %llu MB, queues of %llu MB, max %llu B per operation\n", BYTES_TO_PRODUCE >> 20, QUEUE_SIZE >> 20, MAX_BYTES_PER_OP);
    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer_thread, NULL);
    pthread_create(&cons, NULL, consumer_thread, NULL);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    
    free(bbq_queue.data);
    free(vbq_queue.data);
    free(abq_queue.data);
    free(lfq_queue.data);
    free(bq_queue.data);
    bbq_queue_free(&bbq_queue);
    vbq_queue_free(&vbq_queue);
    abq_queue_free(&abq_queue);

    puts("Test ended without errors");

    prf_output_measures(stdout);

    return 0;
}

static void *producer_thread(void *arg)
{
    (void)arg;
    
    size_t bbq_remaining = BYTES_TO_PRODUCE;
    size_t vbq_remaining = BYTES_TO_PRODUCE;
    size_t abq_remaining = BYTES_TO_PRODUCE;
    size_t lfq_remaining = BYTES_TO_PRODUCE;
    size_t bq_remaining = BYTES_TO_PRODUCE;

    while (bbq_remaining > 0 || vbq_remaining > 0 || abq_remaining > 0 ||
        lfq_remaining > 0 || bq_remaining > 0)
    {
        usleep(rand() % MAX_SLEEP_USEC);
        
        if (bbq_remaining > 0)
        {
            size_t target = bbq_remaining - MIN(bbq_remaining, MAX_BYTES_PER_OP);
            TIME("BBQ push")
                for (; bbq_remaining > target && bbq_queue_push(&bbq_queue, bbq_remaining);
                    bbq_remaining--);
        }

        if (vbq_remaining > 0)
        {
            size_t count = MIN(MAX_BYTES_PER_OP, vbq_remaining);
            uint8_t *bytes = malloc(count);
            assert(bytes);
            for (size_t i = 0; i < count; i++)
                bytes[i] = vbq_remaining - i;

            TIME("VBQ push")
                vbq_remaining -= vbq_queue_push_vector(&vbq_queue, bytes, count);

            free(bytes);
        }

        if (abq_remaining > 0)
        {
            size_t count = MIN(MAX_BYTES_PER_OP, abq_remaining);
            uint8_t *bytes = malloc(count);
            assert(bytes);
            for (size_t i = 0; i < count; i++)
                bytes[i] = abq_remaining - i;

            TIME("ABQ push")
            {
                size_t pushable;
                uint8_t *addr = abq_queue_get_push_buf(&abq_queue, &pushable);
                count = MIN(count, pushable);
                memcpy(addr, bytes, count);
                abq_queue_commit_push(&abq_queue, count);
            }
            abq_remaining -= count;
            
            free(bytes);
        }
        
        if (lfq_remaining > 0)
        {
            size_t count = MIN(MAX_BYTES_PER_OP, lfq_remaining);
            uint8_t *bytes = malloc(count);
            assert(bytes);
            for (size_t i = 0; i < count; i++)
                bytes[i] = lfq_remaining - i;

            TIME("LFQ push")
            {
                size_t pushable;
                uint8_t *addr = lfq_queue_get_push_buf(&lfq_queue, &pushable);
                count = MIN(count, pushable);
                memcpy(addr, bytes, count);
                lfq_queue_commit_push(&lfq_queue, count);
            }
            lfq_remaining -= count;

            free(bytes);
        }
        
        if (bq_remaining > 0)
        {
            size_t count = MIN(MAX_BYTES_PER_OP, bq_remaining);
            uint8_t *bytes = malloc(count);
            assert(bytes);
            for (size_t i = 0; i < count; i++)
                bytes[i] = bq_remaining - i;

            TIME("BQ push")
            {
                size_t pushable;
                uint8_t *addr = bq_pushbuf(&bq_queue, &pushable);
                count = MIN(count, pushable);
                memcpy(addr, bytes, count);
                bq_push(&bq_queue, count);
            }
            bq_remaining -= count;

            free(bytes);
        }
    }

    __atomic_store_n(&prod_finished, true, __ATOMIC_RELEASE);

    return NULL;
}

static void *consumer_thread(void *arg)
{
    (void)arg;

    size_t bbq_remaining = BYTES_TO_PRODUCE;
    size_t vbq_remaining = BYTES_TO_PRODUCE;
    size_t abq_remaining = BYTES_TO_PRODUCE;
    size_t lfq_remaining = BYTES_TO_PRODUCE;
    size_t bq_remaining = BYTES_TO_PRODUCE;

    while (bbq_remaining > 0 || vbq_remaining > 0 || abq_remaining > 0 ||
        lfq_remaining > 0 || bq_remaining > 0 || !prod_finished)
    {
        usleep(rand() % MAX_SLEEP_USEC);

        if (bbq_remaining > 0)
        {
            size_t target = bbq_remaining - MIN(bbq_remaining, MAX_BYTES_PER_OP);
            TIME("BBQ pop")
            {
                uint8_t val;
                for (; bbq_remaining > target && bbq_queue_pop(&bbq_queue, &val); bbq_remaining--)
                    assert(val == (uint8_t)bbq_remaining);
            }
        }

        if (vbq_remaining > 0)
        {
            size_t count = MIN(MAX_BYTES_PER_OP, vbq_remaining);
            uint8_t *bytes = malloc(count);
            assert(bytes);
            
            size_t res;
            TIME("VBQ pop")
                res = vbq_queue_pop_vector(&vbq_queue, bytes, count);

            for (size_t i = 0; i < res; i++)
                assert(bytes[i] == (uint8_t)(vbq_remaining - i));
            
            vbq_remaining -= res;

            free(bytes);
        }

        if (abq_remaining > 0)
        {
            size_t count;
            uint8_t *addr;
            TIME("ABQ get pop buf")
                addr = abq_queue_get_pop_buf(&abq_queue, &count);

            count = MIN(count, abq_remaining);
            count = MIN(count, MAX_BYTES_PER_OP);

            for (size_t i = 0; i < count; i++)
                assert(addr[i] == (uint8_t)(abq_remaining--));
            
            TIME("ABQ commit pop")
                abq_queue_commit_pop(&abq_queue, count);
        }

        if (lfq_remaining > 0)
        {
            size_t count;
            uint8_t *addr;
            TIME("LFQ get pop buf")
                addr = lfq_queue_get_pop_buf(&lfq_queue, &count);

            count = MIN(count, lfq_remaining);
            count = MIN(count, MAX_BYTES_PER_OP);

            for (size_t i = 0; i < count; i++)
                assert(addr[i] == (uint8_t)(lfq_remaining--));
            
            TIME("LFQ commit pop")
                lfq_queue_commit_pop(&lfq_queue, count);
        }

        if (bq_remaining > 0)
        {
            size_t count;
            uint8_t *addr;
            TIME("BQ get pop buf")
                addr = bq_popbuf(&bq_queue, &count);

            count = MIN(count, bq_remaining);
            count = MIN(count, MAX_BYTES_PER_OP);

            for (size_t i = 0; i < count; i++)
                assert(addr[i] == (uint8_t)(bq_remaining--));
            
            TIME("BQ commit pop")
                bq_pop(&bq_queue, count);
        }
    }

    return NULL;
}

PROFILER_GLOBAL_END;