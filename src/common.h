#pragma once

#define CPU_PAUSE() __asm volatile("pause" :::)
#define CACHELINE_SIZE 64
