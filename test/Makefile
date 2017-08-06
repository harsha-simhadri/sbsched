include ../config.mk
include ./knobs

SOURCES = Thread.cc ThreadPool.cc Job.cc Fork.cc Scheduler.cc Thread.hh ThreadPool.hh Job.hh Fork.hh Scheduler.hh 
OBJECTS = Thread.o ThreadPool.o Job.o Fork.o Scheduler.o test.o

HOSTNAME = $(shell uname -n | tr "." "\n" | head -n 1) 

MEMFLAGS = -D HUGE_PAGE_SIZE=$(HUGE_PAGE_SIZE) -D STRIPE_SOCKETS=$(STRIPE_SOCKETS)
DFLAGS = -D FIND_MACHINE=$(HOSTNAME) -D STRAND_SIZE_MODE=$(STRAND_SIZE_MODE) $(MEMFLAGS)

CPFLAGS = $(CFLAGS) $(PFLAGS)

EXECS = GatherScatter SimulatedMM Map RRM RRG RGS RScan quickSort quickSort2 awareSampleSort sampleSort test matMul mklMatMul quadTreeSort quadTreeSort2  # thrtest intSort jTest numProcTest testprof
CILK_EXECS = Cilk-RRM Cilk-RRG

%.o:	%.cc collect.hh matMul.hh quickSort.hh quickHull.hh quickSort2.hh common.hh sequence.hh sequence-jobs.hh transpose.hh intSort.hh sampleSort.hh quadTreeSort.hh quadTreeSort2.hh libperf.h getperf.hh affinity.hh parse-args.hh machine-config.hh
	$(CCP) -c $(CFLAGS) $(DFLAGS) $(PFLAGS) $(IFLAGS) $< -o $@ 

all:	$(EXECS) $(CILK_EXECS)

GatherScatter:	../$(LIBVER)  machine-config.hh GatherScatter.cc GatherScatter.o 
	$(CCP) $(CPFLAGS) -o GatherScatter GatherScatter.o ../$(LIBVER)  $(LFLAGS)

SimulatedMM:	../$(LIBVER)  machine-config.hh SimulatedMM.cc SimulatedMM.o
	$(CCP) $(CPFLAGS) -o SimulatedMM SimulatedMM.o ../$(LIBVER)  $(LFLAGS)

Map:	../$(LIBVER)  machine-config.hh Map.cc Map.o
	$(CCP) $(CPFLAGS) -o Map Map.o ../$(LIBVER)  $(LFLAGS)

RRM:	../$(LIBVER)  machine-config.hh RRM.cc RRM.o
	$(CCP) $(CPFLAGS) -o RRM RRM.o ../$(LIBVER)  $(LFLAGS)

RRG:	../$(LIBVER)  machine-config.hh RRG.cc RRG.o
	$(CCP) $(CPFLAGS) -o RRG RRG.o ../$(LIBVER)  $(LFLAGS)

awareSampleSort:	../$(LIBVER) machine-config.hh awareSampleSort.cc awareSampleSort.o
	$(CCP) $(CPFLAGS) -o awareSampleSort awareSampleSort.o ../$(LIBVER)  $(LFLAGS)

nawareSampleSort:	../$(LIBVER) machine-config.hh nawareSampleSort.cc nawareSampleSort.o
	$(CCP) $(CPFLAGS) -o nawareSampleSort nawareSampleSort.o ../$(LIBVER)  $(LFLAGS)

RGS:	../$(LIBVER)  machine-config.hh RGS.cc RGS.o
	$(CCP) $(CPFLAGS) -o RGS RGS.o ../$(LIBVER)  $(LFLAGS)

RScan:	../$(LIBVER)  machine-config.hh RScan.cc RScan.o
	$(CCP) $(CPFLAGS) -o RScan RScan.o ../$(LIBVER)  $(LFLAGS)

matMul:	../$(LIBVER) machine-config.hh matMul.cc matMul.o 
	$(CCP) $(CPFLAGS) -o matMul matMul.o ../$(LIBVER)    $(LFLAGS)

mklMatMul:	../$(LIBVER) machine-config.hh mklMatMul.cc mklMatMul.o 
	$(CCP) $(CPFLAGS) -o mklMatMul mklMatMul.o ../$(LIBVER)    $(LFLAGS) $(MKL_LFLAGS)

thrtest:	../$(LIBVER)  machine-config.hh main.cc main.o 
	$(CCP) $(CPFLAGS) -o thrtest main.o ../$(LIBVER)    $(LFLAGS)

quickSort:	../$(LIBVER)  machine-config.hh quickSort.hh quickSort.cc quickSort.o 
	$(CCP) $(CPFLAGS) -o quickSort quickSort.o ../$(LIBVER)    $(LFLAGS)

quickSort2:	../$(LIBVER)  machine-config.hh quickSort2.hh quickSort2.cc quickSort2.o 
	$(CCP) $(CPFLAGS) -o quickSort2 quickSort2.o ../$(LIBVER)    $(LFLAGS)

quickHull:	../$(LIBVER)  machine-config.hh quickHull.hh quickHull.cc quickHull.o 
	$(CCP) $(CPFLAGS) -o quickHull quickHull.o ../$(LIBVER)    $(LFLAGS)

quadTreeSort:	../$(LIBVER)  machine-config.hh quadTreeSort.hh quadTreeSort.cc quadTreeSort.o 
	$(CCP) $(CPFLAGS) -o quadTreeSort quadTreeSort.o ../$(LIBVER)    $(LFLAGS)

quadTreeSort2:	../$(LIBVER)  machine-config.hh quadTreeSort2.hh quadTreeSort2.cc quadTreeSort2.o 
	$(CCP) $(CPFLAGS) -o quadTreeSort2 quadTreeSort2.o ../$(LIBVER)    $(LFLAGS)

sampleSort:	../$(LIBVER)  machine-config.hh sampleSort.cc sampleSort.o 
	$(CCP) $(CPFLAGS) -o sampleSort sampleSort.o ../$(LIBVER)   $(LFLAGS)

intSort:	../$(LIBVER)  machine-config.hh intSort.hh intSort.cc intSort.o 
	$(CCP) $(CPFLAGS) -o intSort intSort.o ../$(LIBVER)    $(LFLAGS)

test:	../$(LIBVER)  machine-config.hh test.cc test.o 
	$(CCP) $(CPFLAGS) -o test test.o ../$(LIBVER)  $(LFLAGS) 

testprof: ../$(LIBVER)  machine-config.hh test.cc test.o
	$(CCP) $(CPFLAGS) -o testprof test.o ../$(LIBVER)  $(LFLAGS) -pg

jTest:	../$(LIBVER)  machine-config.hh jTest.cc jTest.o 
	$(CCP) $(CPFLAGS) -o jTest jTest.o ../$(LIBVER)   $(LFLAGS)

numProcTest:	../$(LIBVER)  machine-config.hh numProcTest.cc numProcTest.o
	$(CCP) $(CPFLAGS) -o numProcTest numProcTest.o ../$(LIBVER)   $(LFLAGS)

Cilk-RRM:	Cilk-RRM.cc
	$(CCP) $(CPFLAGS) $(CILK_FLAGS) $(MEMFLAGS) -I ../IntelPCM/ -o Cilk-RRM Cilk-RRM.cc $(COUNTEROBJECTS)

Cilk-RRG:	Cilk-RRG.cc
	$(CCP) $(CPFLAGS) $(CILK_FLAGS) $(MEMFLAGS) -I ../IntelPCM/ -o Cilk-RRG Cilk-RRG.cc $(COUNTEROBJECTS)

clean:
	rm -rf *.o *~ thrtest $(EXECS) $(CILK_EXECS)

