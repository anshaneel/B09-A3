#ifndef STATS_FUNCTIONS_H
#define STATS_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include <utmp.h>
#include <errno.h>


typedef struct memory {

    double total_memory;
    double used_memory;
    double total_virtual;
    double used_virtual;

} memory;

typedef struct cpu_stats {
    
    long int user;
    long int nice;
    long int system;
    long int idle;
    long int iowait;
    long int irq;
    long int softirq;

} cpu_stats;

void headerUsage(int samples, int tdelay);

void footerUsage();

void memoryGraphicsOutput(char memoryGraphics[1024], double memory_current, double* memory_previous, int i);

void memoryStats(int pipefd[2]);

void systemOutput(char terminal[1024][1024], bool graphics, int i, double* memory_previous, memory info);

void userOutput(int pipefd[2]);

void cpuStats(int pipefd[2]);

void CPUGraphics(char terminal[1024][1024], double usage, int i);

void CPUOutput(char terminal[1024][1024], bool graphics, int i, long int* cpu_previous, long int* idle_previous, cpu_stats info);

#endif // STATS_FUNCTIONS_H
