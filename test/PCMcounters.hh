/*
Copyright (c) 2009-2012, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// written by Roman Dementiev
//

#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include "../IntelPCM/cpucounters.h"
#include "../IntelPCM/cpuasynchcounter.h"
#include "../IntelPCM/client_bw.h"
#include <iostream>
#include <list>
#include <vector>
#include <algorithm>
#include <sys/time.h>


using std::cout;
using std::endl;

inline double my_timestamp()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return double(tp.tv_sec) + tp.tv_usec / 1000000.;
}

SystemCounterState before_sstate, after_sstate;
double before_ts, after_ts;

void
printDiff (int N) {
    cout << "\nruntime: " << (after_ts - before_ts) * 1000. << " ms " << std::endl;
    cout << "runtime per element: " << (after_ts - before_ts) * 1000000000. / N << " ns " << std::endl;
    cout << "Num. L2 misses: "<< getL2CacheMisses(before_sstate, after_sstate)/1000 << "K "<<std::endl;
    cout << "Num. L3 misses: "<< getL3CacheMisses(before_sstate, after_sstate)/1000 << "K "<<std::endl;
    cout << "KB written to mem_cntlr: "<<getBytesWrittenToMC(before_sstate, after_sstate)/1024.<<"KB"<<std::endl;
    cout << "KB read from mem_cntlr: " <<getBytesReadFromMC(before_sstate, after_sstate)/1024.<<"KB"<<std::endl;
    cout << "Instructions retired: " << getInstructionsRetired(before_sstate, after_sstate) / 1000000 << "mln" << std::endl;
    cout << "CPU cycles: " << getCycles(before_sstate, after_sstate) / 1000000 << "mln" << std::endl;
    cout << "Instructions per cycle: " << getCoreIPC(before_sstate, after_sstate) << std::endl;
    for (int i=0;  i<m->getNumSockets(); ++i)
      cout << "Cbox_"<<i<<" sum: "<<getCboxSum (i, before_sstate, after_sstate)<<std::endl;
}

PCM* init_pcm () {
  PCM *m = PCM::getInstance();

  if (!m->good())    {
      cout << "Can not access CPU counters" << endl;
      cout << "Try to execute 'modprobe msr' as root user and then" << endl;
      cout << "you also must have read and write permissions for /dev/cpu/?/msr devices (the 'chown' command can help).";
      return -1;
  }
  if(m->program() != PCM::Success){ 
    cout << "Program was not successful..." << endl;
    delete m;
    return -1;
  }	  
  return m;
}

void cleanup_pcm (PCM *m) {
  
  m->cleanup();
  
}
