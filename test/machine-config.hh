#include <unistd.h>

#define SEQUENTIAL int num_procs=1;		\
  int num_levels = 2;				\
  int fan_outs[2] = {1,1};			\
  lluint sizes[2] = {0, 1};			\
  int block_sizes[2] = {8, 8};			\
  uint map[1] = {0};		

#define SEQUENTIAL_TEACH int num_procs=1;		\
  int num_levels = 4;					\
  int fan_outs[4] = {1,1,1,1};				\
  lluint sizes[4] = {0,3*(1<<23), 1<<18, 1<<15};	\
  int block_sizes[4] = {64,64,64,64};			\
  uint map[1] = {0};		

#define SIMPLE int num_procs=2;			\
  int num_levels = 2;				\
  int fan_outs[2] = {2,1};			\
  lluint sizes[2] = {0, 1};			\
  int block_sizes[2] = {64, 64};		\
  uint map[2] = {0,1};		

#define oblivious int num_procs=8;		\
  int num_levels = 4;				\
  int fan_outs[4] = {2,4,1,1};			\
  lluint sizes[4] = {0, 1<<23, 1<<18, 1<<15};	\
  int block_sizes[4] = {64,64,64,64};		\
  uint map[8] = {0,1,2,3,4,5,6,7};		

#define multi6 int num_procs=8;		\
  int num_levels = 4;				\
  int fan_outs[4] = {2,4,1,1};			\
  lluint sizes[4] = {0, 1<<23, 1<<18, 1<<15};	\
  int block_sizes[4] = {64,64,64,64};		\
  uint map[8] = {0,1,2,3,4,5,6,7};	

#define monster int num_procs=32;		\
  int num_levels = 4;				\
  int fan_outs[4] = {4,8,1,1};				\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};	\
  int block_sizes[4] = {64,64,64,64};			\
  uint map[32] = {0,4,8,12,16,20,24,28,			\
		  2,6,10,14,18,22,26,30,		\
		  1,5,9,13,17,21,25,29,			\
		  3,7,11,15,19,23,27,31};

#define monster_4x16 int num_procs=64;					\
  int num_levels = 3;							\
  int fan_outs[3] = {4,16,1};						\
  lluint sizes[3] = {0, 3*(1<<23), 1<<18};				\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[64] = {0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,		\
		  2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,	\
		  1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,		\
		  3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63};


#define PRIVATE int num_procs=8;		\
  int num_levels = 2;				\
  int fan_outs[2] = {8,1};			\
  lluint sizes[2] = {0, 1<<22};			\
  int block_sizes[2] = {64, 64};		\
  uint map[8] = {0,1,2,3,4,5,6,7};		

#define SHARED int num_procs=8;			\
  int num_levels = 2;				\
  int fan_outs[2] = {1,8};			\
  lluint sizes[2] = {0, 1<<23};			\
  int block_sizes[2] = {8, 8};			\
  uint map[8] = {0,1,2,3,4,5,6,7};		

#define DUBIOUS int num_procs=32;		\
  int num_levels = 4;				\
  int fan_outs[4] = {2,2,4,2};			\
  lluint sizes[4] = {0, 1<<23, 1<<18, 1<<15};	\
  int block_sizes[4] = {64, 64, 64, 64};				\
  uint map[8] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};		

#define TEDIOUS int num_procs=8;		\
  int num_levels = 4;				\
  int fan_outs[4] = {2,2,2,1};			\
  lluint sizes[4] = {0, 1<<23, 1<<18, 1<<15};	\
  int block_sizes[4] = {8, 8, 8, 8};		\
  uint map[8] = {0,1,2,3,4,5,6,7};		

#define aware int num_procs=48;				\
  int num_levels = 4;					\
  int fan_outs[4] = {4,12,1,1};				\
  lluint sizes[4] = {0, 3*(1<<22), 1<<19, 1<<16};	\
  int block_sizes[4] = {64,64,64,64};			\
  uint map[48] = {0,4,8,12,16,20,24,28,32,36,40,44,	\
		 3,7,11,15,19,23,27,31,35,39,43,47,	\
		 2,6,10,14,18,22,26,30,34,38,42,46,	\
		 1,5,9,13,17,21,25,29,33,37,41,45};

#define TEACH_HT int num_procs=64;					\
  int num_levels = 4;							\
  int fan_outs[4] = {4,8,1,2};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64, 64, 64, 64};				\
  uint map[64] = {0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,		\
		  2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,	\
		  1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,		\
		  3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63};

#define teach_ht int num_procs=64;					\
  int num_levels = 3;							\
  int fan_outs[4] = {4,16,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64, 64, 64, 64};				\
  uint map[64] = {0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,		\
		  2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,	\
		  1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,		\
		  3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63};

#define teach_st int num_procs=32;					\
  int num_levels = 3;							\
  int fan_outs[4] = {4,8,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[32] = {0,4,8,12,16,20,24,28,					\
		  2,6,10,14,18,22,26,30,				\
		  1,5,9,13,17,21,25,29,					\
		  3,7,11,15,19,23,27,31}; 

//  int num_levels = 4;							
#define teach_onecore int num_procs=1;					\
  int num_levels = 3;							\
  int fan_outs[4] = {1,1,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[1] = {0};

#define teach_onesocket int num_procs=8;					\
  int num_levels = 3;							\
  int fan_outs[4] = {1,8,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[8] = {0,4,8,12,16,20,24,28};

#define teach_twosockets int num_procs=16;					\
  int num_levels = 3;							\
  int fan_outs[4] = {2,8,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[16] = {0,4,8,12,16,20,24,28,					\
		 1,5,9,13,17,21,25,29};

#define teach_threesockets int num_procs=24;					\
  int num_levels = 3;							\
  int fan_outs[4] = {3,8,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[24] = {0,4,8,12,16,20,24,28,					\
		 2,6,10,14,18,22,26,30,					\
		 1,5,9,13,17,21,25,29};
		
		  
#define teach_flat int num_procs=32;					\
  int num_levels = 3;							\
  int fan_outs[3] = {4,8,1};						\
  lluint sizes[3] = {0, 3*(1<<23), 1<<18};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[32] = {0,4,8,12,16,20,24,28,					\
		  2,6,10,14,18,22,26,30,				\
		  1,5,9,13,17,21,25,29,					\
		  3,7,11,15,19,23,27,31}; 

#define teach_4x1 int num_procs=4;					\
  int num_levels = 3;							\
  int fan_outs[3] = {4,1,1};						\
  lluint sizes[3] = {0, 3*(1<<23), 1<<18};\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[4] = {0,							\
		 2,							\
		 1,							\
		 3};

#define teach_4x2 int num_procs=8;					\
  int num_levels = 3;							\
  int fan_outs[4] = {4,2,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[8] = {0,4,							\
		 2,6,							\
		 1,5,							\
		 3,7};


#define teach_4x4 int num_procs=16;					\
  int num_levels = 3;							\
  int fan_outs[4] = {4,4,1,1};						\
  lluint sizes[4] = {0, 3*(1<<23), 1<<18, 1<<15};			\
  int block_sizes[4] = {64,64,64,64};					\
  uint map[16] = {0,4,8,12,						\
		  2,6,10,14,						\
		  1,5,9,13,						\
		  3,7,11,15};

#define teach_4x16 int num_procs=64;					\
  int num_levels = 3;							\
  int fan_outs[3] = {4,16,1};						\
  lluint sizes[3] = {0, 3*(1<<23), 1<<18};			\
  int block_sizes[4] = {64, 64, 64, 64};				\
  uint map[64] = {0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,		\
		  2,6,10,14,18,22,26,30,34,38,42,46,50,54,58,62,	\
		  1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61,		\
		  3,7,11,15,19,23,27,31,35,39,43,47,51,55,59,63};

#define teach teach_ht
