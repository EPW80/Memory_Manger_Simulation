# Memory Management Simulation

## Overview

This program simulates a memory management system using paging and translation look-aside buffer (TLB). It tests virtual to physical memory address translation using either First-In-First-Out (FIFO) or Least Recently Used (LRU) page replacement policies.

## Features

- Page Replacement Policies: FIFO and LRU.
- Memory Components:
- Page table.
- TLB (Translation Look-Aside Buffer).
- RAM simulation.
- Simulation of Physical Memory: Using an array to simulate RAM.
- File Handling: Reads addresses and correct values from files for simulation.
- Performance Metrics: Calculation of Page Fault Percentage and TLB Hit Percentage.

## Configuration

- FRAME_SIZE: The size of each frame in the simulated RAM.
- REPLACE_POLICY: Set to either FIFO or LRU to choose the page replacement policy.
- NFRAMES, PTABLE_SIZE, TLB_SIZE: Sizes for frames, page table, and TLB.

## Usage

- Compilation: Use a C++ compiler to compile the mem_mgr.cpp file.
- Running: Execute the compiled program. Ensure addresses.txt, correct.txt, and BACKING_STORE.bin are present in the same directory.

## Files required

- addresses.txt: Contains virtual addresses to be translated.
- correct.txt: Contains the correct physical addresses and values for validation.
- BACKING_STORE.bin: Simulates the backing storage.

## Output

The program outputs the results of the address translations, including page fault and TLB hit rates, and a summary of the simulation's accuracy.

## Limitations

- This simulation assumes a specific environment and memory size.
- It is designed for educational purposes and may not reflect all aspects of real-world memory management systems.

## Demo

![demo](./demo_memmgr.gif)

## Note

Ensure that the necessary input files are correctly formatted and available in the same directory as the executable for the simulation to run properly.

## Contributor

Erik Williams
