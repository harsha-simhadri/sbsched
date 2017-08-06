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
#include "test.h"
#include "loops.h"

using std::cout;
using std::endl;

void runtest(std::string name, testFunction f, int N, int repeats) {

    before_sstate = getSystemCounterState();
    before_ts = my_timestamp();

    (*f)(N,repeats);

    after_sstate = getSystemCounterState();
    after_ts = my_timestamp();

    std::cout<<"----|"<<name<<"|--|"<<N<<"|--|"<<repeats<<"|----";
    printDiff();
    std::cout<<"---------------------------------\n"<<std::endl;
}

int main(int argc, char * argv[])
{
    #define E double
    int N = atoi(argv[1]);
    initloop(N);

    initPCM();
    
    cout << std::endl << std::endl << "Num elements: " << N << std::endl;
    cout << "Elements data size: " << sizeof(uint64_t) * N / 1024 << " KB" << std::endl;

    runtest ("Sum loop",sumloop,N,1);
    flush_cache(1ULL<<25);
    runtest ("Sum loop",sumloop,N,3);
    flush_cache(1ULL<<25);
    runtest ("A->B loop",copyloop,N,3);
    flush_cache(1ULL<<25);
    runtest ("A->A loop",transformloop,N,1); 
    flush_cache(1ULL<<25);
    runtest ("A->A loop",transformloop,N,3); 


    m->cleanup();
    return 0;
}
