#include "gettime.hh"
#include <iostream>

int main () {
  int N = 100;
  
  uint64_t timestamps[N];
 
  for (volatile int i=0; i<N; ++i) {
    timestamps[i] = getticks();
    sleep_for_nanoseconds(10000000L);
  }
  for (int i=0; i<N; ++i)
    std::cout<<timestamps[i]-timestamps[0]<<std::endl;
}
