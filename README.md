# Byte Queue

A high performance, MT-safe, lockfree and branchless circular byte buffer for SPSC in 50 loc.

## Key Features

- **Zero-copy API**: commit based api with direct access to internal buffers to avoid unnecessary memory copies.
- **MT-safe and Lock-free**: Safe for a Suitable for Single Producer Single Consumer scenario without any locking.
- **Fast**: No divisions, no modulo operations and no branches to go up to **1000x faster** than naive implementations

## Repository Structure

- `bq.h`: Header file with the **byte queue (bq)** implementation.
- `profiler.h`: Profiler code used for performance measure.
- `test.c`: Test code comparing BQ with other implementations and testing their correctness.
- `others/`: Folder containing five other implementations for comparison.

## Usage

Include `bq.h` in your sources.

## Further Reading

This implementation comes from a detailed design journey, explained step by step in [this article](https://delgaudio.me/articles/bq.html).