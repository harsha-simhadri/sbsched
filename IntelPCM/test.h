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
#include "cpucounters.h"
#include "cpuasynchcounter.h"
#include "client_bw.h"
#include <iostream>
#include <sys/time.h>

using std::cout;
using std::endl;

SystemCounterState before_sstate, after_sstate;
double before_ts, after_ts;

PCM *m;

inline double my_timestamp()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return double(tp.tv_sec) + tp.tv_usec / 1000000.;
}

void
printDiff () {
  cout << "T: " << (int)((after_ts - before_ts) * 1000.) << " ms / ";
  cout << "L2_M: "<< getL2CacheMisses(before_sstate, after_sstate)/1000000 << "mln / ";
  cout << "L3_M: "<< getL3CacheMisses(before_sstate, after_sstate)/1000000 << "mln "<<std::endl;
  cout << "Mem-cntlr: Rd "<<getBytesWrittenToMC(before_sstate, after_sstate)/1024./1024.<<"MB / ";
  cout << "Wr " <<getBytesReadFromMC(before_sstate, after_sstate)/1024./1024.<<"MB"<<std::endl;
  cout << "Instr_rtrd: " << getInstructionsRetired(before_sstate, after_sstate) / 1000000 << "mln / ";
  cout << "CPU_cycles: " << getCycles(before_sstate, after_sstate) / 1000000 << "mln" << std::endl;
  cout << "Instructions per cycle: " << getCoreIPC(before_sstate, after_sstate) << std::endl;

  long long int count=0;
  for (int i=0;  i<m->getNumSockets(); ++i) 
      count+=getCboxSum (i, before_sstate, after_sstate,0);
  cout << "L3_MISSES: "<<count<<endl;
  for (int ctr=0;ctr<MAX_CBOX_COUNTERS;++ctr) {
    long long int sum=0; std::cout<<"Cbox ("; 
    for (int i=0;  i<m->getNumSockets(); ++i) {
      long long int count=getCboxSum (i, before_sstate, after_sstate,ctr);
    sum+=count;
    std::cout<<" "<<count/1000000.;
    }
    std::cout<<" ) = "<<sum/1000000.<<"mln"<<std::endl; 
  }
}

void initPCM () {
    m = PCM::getInstance();
    if (!m->good())    {
        cout << "Can not access CPU counters" << endl;
        cout << "Try to execute 'modprobe msr' as root user and then" << endl;
        cout << "you also must have read and write permissions for /dev/cpu/?/msr devices (the 'chown' command can help).";
	exit(-1);
    }
    if(m->program() != PCM::Success){ 
	cout << "Program was not successful..." << endl;
	delete m;
	exit(-1);
    }	  
}

void closePCM() {
  m->cleanup();
}
