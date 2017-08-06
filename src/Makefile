
include ../config.mk

HEADERS = Thread.hh ThreadPool.hh Fork.hh Job.hh Scheduler.hh syncQueue.hh HR1Scheduler.hh HR2Scheduler.hh HR3Scheduler.hh HR4Scheduler.hh $(COUNTERDIR)/test.h
IMPLEMENTATION = Thread.cc DecentralThreadPool.cc DecentralFork.cc Job.cc DecentralScheduler.cc WSScheduler.cc HR1Scheduler.cc HR2Scheduler.cc HR3Scheduler.cc HR4Scheduler.cc 
MONITORS =  gettime.hh threadTimers.hh 


SOURCES = $(HEADERS) $(IMPLEMENTATION) $(MONITORS)

COMMONOBJECTS = Thread.o Job.o
DECENTRALOBJECTS = $(COMMONOBJECTS) HR1Scheduler.o HR2Scheduler.o HR3Scheduler.o HR4Scheduler.o  WSScheduler.o DecentralThreadPool.o DecentralFork.o DecentralScheduler.o
OBJECTS = $(COMMONOBJECTS)  Fork.o Scheduler.o

all: decentrallibthrpool.a 

%.o:	%.cc $(SOURCES) #%.hh
	$(CC) -c $(CFLAGS) $(PFLAGS) $(IFLAGS) $< -o $@ 

decentrallibthrpool.a:	$(SOURCES) $(DECENTRALOBJECTS)
	$(AR) ../decentrallibthrpool.a $(DECENTRALOBJECTS) $(COUNTEROBJECTS)
	$(RANLIB) ../decentrallibthrpool.a

#libthrpool.a:	$(SOURCES) $(OBJECTS)
#	$(AR) ../libthrpool.a $(OBJECTS)
#	$(RANLIB) ../libthrpool.a

clean:
	rm -rf *.o *~ decentrallibthrpool.a

