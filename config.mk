CFLAGS	=	-D_REENTRANT -O 
LFLAGS	=	-lpthread -lrt -lm #-ltcmalloc
MKL_LFLAGS =	-L $(MKLROOT)/lib/intel64 -lmkl_intel_lp64 -lmkl_sequential -lmkl_core
IFLAGS  =       -I../src -I../IntelPCM -I$(MKLROOT)/include 
LIBVER	=       decentrallibthrpool.a
AR	=	ar rc
RANLIB	=	ranlib

CILK_INCLUDE=	$(GCCDIR)/include/cilk/
CILK_LIB= 	$(GCCDIR)/lib/
CILK_FLAGS=	-lcilkrts -fcilkplus

COUNTERDIR = ../IntelPCM
COUNTEROBJECTS = $(COUNTERDIR)/msr.o $(COUNTERDIR)/pci.o $(COUNTERDIR)/cpucounters.o $(COUNTERDIR)/client_bw.o

CC	=	$(GCCDIR)/bin/gcc
CCP	=	$(GCCDIR)/bin/g++

#CFLAGS	=	-D_REENTRANT -ggdb
#CFLAGS	=	-D_REENTRANT -DNBEBUG -ggdb -O2 -funroll-loops  -finline-functions
#CFLAGS	=	-D_REENTRANT -DNDEBUG -O2 -funroll-loops -finline-functions
CFLAGS	=	-D_REENTRANT -DNDEBUG -O3 -funroll-loops -finline-functions -fno-omit-frame-pointer
