// -*- C++ -*-

#ifndef __GETTIME_HH
#define __GETTIME_HH

/* 
 * This simple utility function hides Linux/Windows differences so that
 * we can provide examples with source code common to the different
 * platforms.
 *
 * Copyright (c) 2007-2008 Cilk Arts, Inc.  55 Cambridge Street,
 * Burlington, MA 01803.  Patents pending.  All rights reserved. You may
 * freely use the sample code to guide development of your own works,
 * provided that you reproduce this notice in any works you make that
 * use the sample code.  This sample code is provided "AS IS" without
 * warranty of any kind, either express or implied, including but not
 * limited to any implied warranty of non-infringement, merchantability
 * or fitness for a particular purpose.  In no event shall Cilk Arts,
 * Inc. be liable for any direct, indirect, special, or consequential
 * damages, or any other damages whatsoever, for any use of or reliance
 * on this sample code, including, without limitation, any lost
 * opportunity, lost profits, business interruption, loss of programs or
 * data, even if expressly advised of or otherwise aware of the
 * possibility of such damages, whether in an action of contract,
 * negligence, tort, or otherwise.
 *
 */

#ifdef _WIN32
#include <Windows.h>
#else
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#endif

/* example_get_time
   get the time in milliseconds.  This means different things in Windows vs.
   Unix.  In Windows, it's a call to GetTickCount() which is the uptime of
   the system.  In Unix, it is implemented with gettimeofday() and sets the
   counter to zero the first time example_get_time is called.

   returns: the number of milliseconds since the start time.
 */
extern "C++"
inline
int example_get_time()
{
#ifdef _WIN32
    // Windows implementation.
    return (int) GetTickCount();
#else
    static timeval* start = NULL;
    struct timezone tzp = {
     0, 0
    };
    if (NULL == start) {
        // define the current time as 0.
        start = (timeval *) malloc(sizeof(timeval));
        gettimeofday(start, &tzp);
        return 0;
    } else {
        // subtract the start time from the current time.
        timeval end;
        long ms = 0;
        gettimeofday(&end, &tzp);
        ms = (end.tv_sec - start->tv_sec) * 1000;
        ms += ((int) end.tv_usec - (int) start->tv_usec) / 1000;
        return (int) ms;
    }
#endif
}

extern "C++"
inline
long int get_time_microsec()
{
#ifdef _WIN32
    // Windows implementation.
    return 0;
#else
    static timeval* start_micros = NULL;
    struct timezone tzp = {
     0, 0
    };
    if (NULL == start_micros) {
        // define the current time as 0.
        start_micros = (timeval *) malloc(sizeof(timeval));
        gettimeofday(start_micros, &tzp);
        return 0;
    } else {
        // subtract the start time from the current time.
        timeval end;
        long micros = 0;
        gettimeofday(&end, &tzp);
        micros = (end.tv_sec - start_micros->tv_sec) * 1000000;
        micros += ((long int) end.tv_usec - (long int) start_micros->tv_usec);
        return (long int) micros;
    }
#endif
}

typedef unsigned long long int ull_t;

__inline__
ull_t get_time_nanosec()
{
  static timespec* start_nanos = NULL;

  if (NULL == start_nanos) {
    start_nanos = (timespec *) malloc(sizeof(timespec));
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, start_nanos);
    return 0;
  } else {
    timespec end;
    long nanos = 0;
    clock_gettime(CLOCK_MONOTONIC, &end);
    nanos = (end.tv_sec - start_nanos->tv_sec)*1000000000L;
    nanos += ((long int) end.tv_nsec - (long int)start_nanos->tv_nsec);
    return nanos;
  }
}

typedef unsigned long long ticks_t;

__inline__
ull_t get_time_clockticks(void)
{
  static unsigned cycles_high, cycles_low; 
  asm volatile ("RDTSCP" : "=a" (cycles_high), "=d"(cycles_low)  : :  "%rbx", "%rcx");
  return ((ull_t)cycles_high) | (((ull_t)cycles_low)<<32);
}

/*
   "RDTSCP\n\t"
    "mov %%edx, %0\n\t"
    "mov %%eax, %1\n\t"
    "CPUID\n\t"
    : "=r" (cycles_high), "=r"(cycles_low)
    :
    : "%rax", "%rbx", "%rcx", "%rdx");

*/
/****** Unreliable because of out-of-order exec **
     asm volatile("rdtsc" : "=a" (a), "=d" (d)); 
     return ((ticks_t)a) | (((ticks_t)d) << 32); 
**************/

/* example_random
   hash the value, n, that is passed using a 32-bit LCG:
   (a * n + b) % c
   The values of a, b, and c, are the same as those used in glibc.  This
   function exists to provide random numbers that are consistent across
   compilers and between 32 and 64 bit platforms.  It will also generate
   consistent results in parallel, provided that each node in the call-
   graph provides a consistent n.

   returns: the hashed value of n in the low 32 bits.
 */
extern "C++"
inline
unsigned int example_random(unsigned int n)
{
    // 32-bit LCG used by glibc.
    // We use this to keep random numbers consistent between 32 and 64 bit
    // platforms and between compilers.
    return (((unsigned int) 1103515245 * n) + (unsigned int) 12345) %
        (unsigned int) 0xFFFFFFFF;
}

static long _t1;

#define timeStatement(_A,_string) _t1 = example_get_time();  _A;   std::cout << _string << ": " << (example_get_time() - _t1) / 1000.f << " seconds" << "; " << std::endl;

#define timeStatementF(_A,_string,_flag) _t1 = example_get_time();  _A; if (_flag) std::cout << _string << ": " << (example_get_time() - _t1) / 1000.f << " seconds" << "; " << std::endl;

#define TS(_string,_flag,_A) if (_flag) _t1 = example_get_time();  _A; if (_flag) std::cout << _string << ": " << (example_get_time() - _t1) / 1000.f << " seconds" << "; " << std::endl;

#define StartTime(_flag) if (_flag) _t1=example_get_time();
#define NextTime(_string,_flag) if (_flag) {std::cout << _string << ": " << (example_get_time() - _t1) / 1000.f << " seconds" << "; " << std::endl; _t1 = example_get_time();}

#define startTime() _t1=example_get_time();
#define nextTime(_string) {std::cout << _string << ": " << (example_get_time() - _t1) << " mseconds" << "; " << std::endl; _t1 = example_get_time();}
#endif // CILK_EXAMPLE_UTIL_GETTIME_H_INCLUDED

