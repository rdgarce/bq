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

#ifndef PROFILER_H
#define PROFILER_H

#include <x86intrin.h>
#include <stdint.h>
#include <stdio.h>

struct measure
{
    char *label;
    uint64_t clocks;
    uint64_t executions;
};

extern struct measure profiler_m__[];

static void prf_output_measures(FILE *stream);

static inline uint64_t prf_mfence_(void)
{
    _mm_mfence();
    return 0;
}

static void prf_measure_(char *label, uint64_t id, uint64_t clocks)
{
    profiler_m__[id].label = label;
    profiler_m__[id].clocks += clocks;
    profiler_m__[id].executions++;
}

#define TIME(label)                                                                             \
for (uint64_t d__ = 0, f__ = prf_mfence_(), s__ = _rdtsc();                                     \
!d__;                                                                                           \
_mm_mfence(), prf_measure_((label), __COUNTER__, _rdtsc() - s__), d__ = 1, (void)f__)

#define PROFILER_GLOBAL_END                                                                     \
struct measure profiler_m__[__COUNTER__ + 1] = {0};                                             \
static void prf_output_measures(FILE *stream)                                                   \
{                                                                                               \
    fputs("====== PROFILER START ======\n", stream);                                            \
    for (size_t i = 0; i < sizeof(profiler_m__) / sizeof(profiler_m__[0]) - 1; i++)             \
    {                                                                                           \
        fprintf(stream, "%s: # Executions: %lu | Tot. clocks: %lu | Avg. clocks/exec: %f\n",    \
        profiler_m__[i].label, profiler_m__[i].executions, profiler_m__[i].clocks,              \
        (double)profiler_m__[i].clocks/profiler_m__[i].executions);                             \
    }                                                                                           \
    fputs("====== PROFILER END ======\n", stream);                                              \
}

#endif