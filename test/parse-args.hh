#include "ThreadPool.hh"
#include "machine-config.hh"
#include "errno.h"
void
print_usage () {
      std::cerr<<"Usage: cmd <Sched:W/P/H/2/3/4> <args>"<<std::endl;	
}

//FIND_MACHINE;
monster_4x16;

Scheduler*
create_scheduler (int argv, char **argc) {

  Scheduler *sched;

  if (argv >= 2) {
    if (*argc[1] == 'B' || *argc[1]=='b')
      sched = new Scheduler(num_procs);
    else if (*argc[1] == 'W' || *argc[1]=='w')
      sched = new WS_Scheduler(num_procs);
    else if (*argc[1] == 'P' || *argc[1]=='p')
      sched = new PWS_Scheduler(num_procs, *fan_outs, 10);
    else if (*argc[1] == 'H' || *argc[1]=='h')
      sched = new HR_Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes);
     else if (*argc[1] == '2')
       sched = new HR2Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes, 0);
     else if (*argc[1] == '3')
      sched = new HR3Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes);
     else if (*argc[1] == '4')
      sched = new HR4Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes);
     else if (*argc[1] == '5')
       sched = new HR2Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes, 1);
     else {
       print_usage();	
       exit(-1);					
     }	
  } else {
    print_usage();
    exit(-1);
  }

  return sched;
}

long long int 
get_size (int argv, char **argc, int pos=2) {
  if (argv>pos)
    return atoi (argc[pos]);
  else 
    return -1;
}
