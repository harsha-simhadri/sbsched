#ifndef _BENCH_UTILS_INCLUDED
#define _BENCH_UTILS_INCLUDED
#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>

#if defined(__APPLE__)
#define PTCMPXCH "  cmpxchgl %2,%1\n"
#else
#define PTCMPXCH "  cmpxchgq %2,%1\n"
#include <malloc.h>
static int __ii =  mallopt(M_MMAP_MAX,0);
static int __jj =  mallopt(M_TRIM_THRESHOLD,-1);
#endif

#define newA(__E,__n) (__E*) malloc((__n)*sizeof(__E))

#define MB_1 (1024*1024)
#define MB_2 (2*MB_1)


/* Set process affinity of calling thread
 * Spawned threads will inherit affinity */
int 
set_proc_affinity (uint proc_id) {
  int status;
  pthread_t thread = pthread_self();
  if ( proc_id != -1 ) {
    cpu_set_t affinity;
    CPU_ZERO ( &affinity );
    CPU_SET ( proc_id, &affinity );
    if ((status = pthread_setaffinity_np ( thread, sizeof(affinity), &affinity))
	!= 0) {
      std::cerr << "Setting thread affinity to "<<proc_id<<" failed with error value "
	//		<< strerror ( status ) <<std::endl;
		<<std::endl;
    } 
  }
  
  return status;
}

int
flush_cache (int procs, int L3_size) {
  int len=L3_size/sizeof(double);
  double *flush = new double[len];
  volatile double sum;
  for (int p=0; p<procs; ++p) {
    set_proc_affinity(p);
    for (int i=0; i<len; ++i)
      sum += ++flush[i];
  }
  delete flush;
  return sum;
}

void
stripe (char *space, int alloc_size, int page_size=-1) {
  if (page_size == -1) 
    page_size = HUGE_PAGE_SIZE;
  int num_pages = alloc_size/page_size;
  for (int i=0; i<STRIPE_SOCKETS; ++i) {
    set_proc_affinity(i);
    for (int j=0; j<num_pages; ++j) {
      if (j%STRIPE_SOCKETS == i) {
	*(space + j*page_size) = 0;
      }
    }
  }
}

int alloc_hugetlb(long long int size)
{
  int shmid = shmget(IPC_PRIVATE, (1 + size>>20)*MB_2, SHM_HUGETLB
		      | IPC_CREAT | SHM_R
		      | SHM_W);
  if ( shmid < 0 ) {
    perror("shmget");
    exit(1);
  }
  printf("HugeTLB shmid: 0x%x\n", shmid);
  return shmid;
}

void* translate_hugetlb (int shmid) {
  void *ret = shmat(shmid, 0, 0);
  if (ret == (void *)-1) {
    perror("Shared memory attach failure");
    shmctl(shmid, IPC_RMID, NULL);
    exit(2);
  }
  return ret;
}

void free_hugetlb(int shmid) {
  shmctl(shmid, IPC_RMID, NULL);
}

namespace utils {

inline int log2(int i) {
  int a=0;
  int b=i-1;
  while (b > 0) {b = b >> 1; a++;}
  return a;
}

inline int nextPower(int i) {
  int a=0;
  int b=i-1;
  while (b > 0) {b = b >> 1; a++;}
  return (1 << a);
}

inline unsigned int hash(unsigned int a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

inline unsigned int hash2(unsigned int a)
{
  return (((unsigned int) 1103515245 * a) + (unsigned int) 12345) %
    (unsigned int) 0xFFFFFFFF;
}

inline void assertPositive (int n) {
  if (n<=0) {
    std::cerr<<"In file: "<<__FILE__
	     <<", line: "<<__LINE__
	     <<", size is: "<<n
	     <<", must be positive."<<std::endl;
    exit(-1);
  }
}

template <class ET>
inline bool CAS(ET **ptr, ET* oldv, ET* newv) {
  unsigned char ret;
  /* Note that sete sets a 'byte' not the word */
  __asm__ __volatile__ (
                "  lock\n"
                //"  cmpxchgq %2,%1\n"
		PTCMPXCH
                "  sete %0\n"
                : "=q" (ret), "=m" (*ptr)
                : "r" (newv), "m" (*ptr), "a" (oldv)
                : "memory");
  return ret;
}

inline bool CAS(long *ptr, long oldv, long newv) {
  unsigned char ret;
  /* Note that sete sets a 'byte' not the word */
  __asm__ __volatile__ (
                "  lock\n"
                "  cmpxchgq %2,%1\n"
                "  sete %0\n"
                : "=q" (ret), "=m" (*ptr)
                : "r" (newv), "m" (*ptr), "a" (oldv)
                : "memory");
  return ret;
}

inline bool CAS(int *ptr, int oldv, int newv) {
  unsigned char ret;
  /* Note that sete sets a 'byte' not the word */
  __asm__ __volatile__ (
                "  lock\n"
                "  cmpxchgl %2,%1\n"
                "  sete %0\n"
                : "=q" (ret), "=m" (*ptr)
                : "r" (newv), "m" (*ptr), "a" (oldv)
                : "memory");
  return ret;
}

inline bool CCAS(int *ptr, int oldv, int newv) {
  unsigned char ret = 0;
  if (*ptr == oldv) {
  /* Note that sete sets a 'byte' not the word */
  __asm__ __volatile__ (
                "  lock\n"
                "  cmpxchgl %2,%1\n"
                "  sete %0\n"
                : "=q" (ret), "=m" (*ptr)
                : "r" (newv), "m" (*ptr), "a" (oldv)
                : "memory");
  }
  return ret;
}

inline bool writeMax(int *a, int b) {
  int c; bool r=0;
  do c = *a; 
  while (c < b && !(r=CAS(a,c,b)));
  return r;
}

inline bool writeMin(int *a, int b) {
  int c; bool r = 0;
  do c = *a; 
  while (c > b && !(r=CAS(a,c,b)));
  return r;
}

inline bool writeMin(long *a, long b) {
  long c; bool r = 0;
  do c = *a; 
  while (c > b && !(r=CAS(a,c,b)));
  return r;
}

static int logUp(unsigned int i) {
  int a=0;
  int b=i-1;
  while (b > 0) {b = b >> 1; a++;}
  return a;
}

static int logUpLong(unsigned long i) {
  int a=0;
  long b=i-1;
  while (b > 0) {b = b >> 1; a++;}
  return a;
}

static void myAssert(int cond, std::string s) {
  if (!cond) {
    std::cout << s << std::endl;
    abort();
  }
}

void flush_cache (int len) {
  double *flush = new double[len]; double sum;
  for (int i=0; i<len; ++i)
    sum += flush[i];
  delete flush;
}

template <class E>
static void printA(E *A, int n, std::string st, int flag) {
  if (flag) {
    std::cout << st << " ";
    for (int i=0; i < n; i++) std::cout << i << ":" << A[i] << " ";
    std::cout << std::endl;
  }
}

template <class E>
struct identityF { E operator() (const E& x) {return x;}};

template <class E>
struct addF { E operator() (const E& a, const E& b) const {return a+b;}};

template <class E>
struct zeroF { E operator() (const E& a) const {return 0;}};

template <class E>
struct maxF { E operator() (const E& a, const E& b) const {return (a>b) ? a : b;}};

template <class E1, class E2>
  struct firstF {E1 operator() (std::pair<E1,E2> a) {return a.first;} };

template <class E1, class E2>
  struct secondF {E2 operator() (std::pair<E1,E2> a) {return a.second;} };

}

  #define _stride 16
  #define _buckets 64

template <class E>
struct memPoolHack {
  E* _A[_stride*_buckets];
  E* _B[_buckets*2];
  E* _C[_buckets];
  int bsizes;
  int round;

  memPoolHack(int words) {
    bsizes = words/(_buckets*2);
    for (int i=0; i < _buckets*2; i++) 
      _B[i] = (E*) malloc(sizeof(E)*bsizes);
    for (int i=0; i < _buckets; i++) 
      _C[i] = _A[i*_stride] = _B[i];
    round = 0;
  }

  memPoolHack() {}

  void flip() {
    if (round & 1) 
      for (int i=0; i < _buckets; i++)
	_C[i] = _A[i*_stride] = _B[i];
    else
      for (int i=0; i < _buckets; i++)
	_C[i] = _A[i*_stride] = _B[i+_buckets];
    round++;
  }

  void stats() {
    int maxb=0; int sumb=0;
    for (int i=0; i < _buckets; i++) {
      int bsize = _A[i*_stride]-_C[i];
      sumb += bsize;
      maxb = (bsize > maxb) ? bsize : maxb;
    }
    std::cout << "Bucket sizes: average = " << sumb/_buckets << " max = " << maxb 
	      << " allocated = " << bsizes << " _buckets = " << _buckets << std::endl;
  }

  void del() {for (int i=0; i < _buckets*2; i++) free(_B[i]);}

  E* newAr(int words,int id) {
    E* v;
    int i = (utils::hash(id)&(_buckets-1));
    do v = _A[i*_stride];
    while (!utils::CAS((long*) &_A[i*_stride], (long) v, (long) (v+words)));
    if ((v+words) > _C[i] + bsizes) {
      std::cout << "memPoolHack: Out of memory" << std::endl;
      abort();
    }
    return v;
  }

};

struct memPoolHackN {
  void* _A[_stride*_buckets];
  void* _B[_buckets*2];
  void* _C[_buckets];
  int bsizes;
  int round;

  memPoolHackN(int bytes) {
    bsizes = bytes/(_buckets*2);
    for (int i=0; i < _buckets*2; i++) 
      _B[i] = (void*) malloc(bsizes);
    for (int i=0; i < _buckets; i++) 
      _C[i] = _A[i*_stride] = _B[i];
    round = 0;
  }

  memPoolHackN() {}

  void flip() {
    if (round & 1) 
      for (int i=0; i < _buckets; i++)
	_C[i] = _A[i*_stride] = _B[i];
    else
      for (int i=0; i < _buckets; i++)
	_C[i] = _A[i*_stride] = _B[i+_buckets];
    round++;
  }

  void stats() {
    int maxb=0; int sumb=0;
    for (int i=0; i < _buckets; i++) {
      int bsize = ((char*) _A[i*_stride])-((char*) _C[i]);
      sumb += bsize;
      maxb = (bsize > maxb) ? bsize : maxb;
    }
    std::cout << "Bucket sizes: average = " << sumb/_buckets << " max = " << maxb 
	      << " allocated = " << bsizes << std::endl;
  }

  void del() {for (int i=0; i < _buckets*2; i++) free(_B[i]);}

  void* newAr(int bytes, int id) {
    char* v;
    int i = (utils::hash(id)&(_buckets-1));
    do v = (char*) _A[i*_stride];
    while (!utils::CAS((long*) &_A[i*_stride], (long) v, (long) (v+bytes)));
    //std::cout << "alloc: " << bytes << " loc: " << ((void *) v) << std::endl;
    if ((v+bytes) > ((char*) _C[i]) + bsizes) {
      std::cout << "memPoolHack: Out of memory" << std::endl;
      abort();
    }
    return v;
  }
};
#endif // _BENCH_UTILS_INCLUDED
