#include <inttypes.h>
#include "libperf.h"
#include "stdio.h"
#include <stdlib.h>
#include <iostream>
//#include "cilk.h"


int main ()
{
  int LEN=1000000;
  int *A = (int*) malloc (sizeof(int)*LEN);
/*
  int num_procs = 32;
  int *fd = new int[num_procs+1];

  
  fd[num_procs] = new_counter (LIBPERF_COUNT_HW_INSTRUCTIONS, -1, -1);
  cilk_for (int i=0; i<num_procs; ++i) {
    fd[i] = new_counter (LIBPERF_COUNT_HW_INSTRUCTIONS, i, fd[num_procs]);
    //enable_counter (fd[i]);
  }
  enable_counter (fd[num_procs]);
  

  for (int i=0; i<LEN; ++i)
    ++A[i];
  
  cilk_for (int i=0; i<LEN; ++i)
    ++A[i];

  disable_counter (fd[num_procs]);

  for (int i=0; i<num_procs; ++i)
    fprintf(stdout, "counter read: %llu \n", read_counter(fd[i]));
  fprintf(stdout, "sum of counter read: %llu \n", read_counter(fd[num_procs]));
   */



	int fds[10];
	int j;
	for (j=0; j<10; ++j) {

		fds[j] = new_counter (LIBPERF_COUNT_HW_INSTRUCTIONS, -1, -1);
		enable_counter (fds[j]);


		for (int i=0; i<LEN; ++i)
			++A[i];

	}


	for (j=0; j<10; ++j)
    		std::cout<< read_counter(fds[j]) <<std::endl;

	for (j=0; j<10; ++j)
		disable_counter (fds[9-j]);
}
