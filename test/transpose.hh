
#ifndef A_TRANSPOSE_INCLUDED
#define A_TRANSPOSE_INCLUDED

#define _TRANS_THRESHHOLD 64
#define _PAR_TRANS_THRESHHOLD (1<<7)

template <class E>
class Transpose : public HR2Job {
  
  E *A, *B;
  int rStart, rCount, rLength, cStart, cCount, cLength;
  int stage;
  
public:
  // Stage 0 constructor
  Transpose (E *A_, E *B_,
	     int rStart_, int rCount_, int rLength_,
	     int cStart_, int cCount_, int cLength_,
	     bool del=true)
    : HR2Job (del),
      A(A_), B(B_),
      rStart (rStart_), rCount(rCount_), rLength (rLength_),
      cStart (cStart_), cCount(cCount_), cLength (cLength_),
      stage (0)
    {}

  // Stage 0 constructor
  Transpose (E *A_, E* B_, int rCount_, int cCount_, bool del=true)
   : HR2Job (del), A(A_), B(B_),
     rStart (0), rCount(rCount_), rLength (cCount_),
     cStart (0), cCount(cCount_), cLength (rCount_),
     stage (0)
    {}

  // Stage 1 constructor
  Transpose (int rCount_, int cCount_,
	     bool del=true)
    : HR2Job (del),
      stage (1),
      rCount (rCount_), cCount (cCount_)
    {}

  lluint size (const int block_size) {
    return rCount*round_up(cCount*sizeof(E), block_size);
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE == 1) {
      return size(block_size);
    } else {
      if (stage==0 && cCount<=_PAR_TRANS_THRESHHOLD && rCount<=_PAR_TRANS_THRESHHOLD) {
	return size(block_size);
      } else {
	return STRAND_SIZE;
      }
    }
  }

  void seq_transpose(int rStart, int rCount, int rLength,
		    int cStart, int cCount, int cLength) {
    if (cCount < _TRANS_THRESHHOLD && rCount < _TRANS_THRESHHOLD) {
      for (int i=rStart; i < rStart + rCount; i++) 
	for (int j=cStart; j < cStart + cCount; j++) 
	  B[j*cLength + i] = A[i*rLength + j];
    } else if (cCount > rCount) {
      int l1 = cCount/2;
      int l2 = cCount - cCount/2;
      seq_transpose(rStart,rCount,rLength,cStart,l1,cLength);
      seq_transpose(rStart,rCount,rLength,cStart + l1,l2,cLength);
    } else {
      int l1 = rCount/2;
      int l2 = rCount - rCount/2;
      seq_transpose(rStart,l1,rLength,cStart,cCount,cLength);
      seq_transpose(rStart + l1,l2,rLength,cStart,cCount,cLength);
    }	
  }
  
  void function() {
    if (stage == 0) {
      if (cCount <= _PAR_TRANS_THRESHHOLD && rCount <= _PAR_TRANS_THRESHHOLD) {
	seq_transpose (rStart,rCount,rLength,cStart,cCount,cLength);
	join();
      } else if (cCount > rCount) {
	int l1 = cCount/2;
	int l2 = cCount - cCount/2;

	binary_fork (new Transpose<E>(A,B,rStart,rCount,rLength,cStart,l1,cLength),
		     new Transpose<E>(A,B,rStart,rCount,rLength,cStart + l1,l2,cLength),
		     new Transpose<E>(rCount, cCount));
      } else {
	int l1 = rCount/2;
	int l2 = rCount - rCount/2;
	binary_fork ( new Transpose<E>(A,B,rStart,l1,rLength,cStart,cCount,cLength),
		      new Transpose<E>(A,B,rStart + l1,l2,rLength,cStart,cCount,cLength),
		      new Transpose<E>(rCount, cCount));
      }
    } else {
      join();
    }
  }
};

template <class E>
class BlockTranspose : public HR2Job {
#define BT_ST_SIZE 12
#define BT_ST_SIZE_JOIN 13
  
  E *A, *B;
  int *OA, *OB, *L;
  int rStart, rCount, rLength, cStart, cCount, cLength;
  int stage;
  lluint *sizes; int node_id; int level;
  lluint **sizes_ptr;
  
public:
  // Constructor for an already sized block transpose
  BlockTranspose(E *AA, E *BB, int *OOA, int *OOB, int *LL,
		 int rStart_, int rCount_, int rLength_,
		 int cStart_, int cCount_, int cLength_,
		 lluint **sizes_ptr_,
		 bool del=true)
    : HR2Job (del),
      A(AA), B(BB), OA(OOA), OB(OOB), L(LL),
      rStart (rStart_), rCount(rCount_), rLength (rLength_),
      cStart (cStart_), cCount(cCount_), cLength (cLength_),
      sizes_ptr (sizes_ptr_),
      sizes(NULL), node_id(0), level (0),
      stage (BT_ST_SIZE)
    {}

  BlockTranspose (E *AA, E *BB, int *OOA, int *OOB, int *LL,
		  int rCount_, int cCount_,
		  lluint **sizes_ptr_,
		  bool del=true)
    : HR2Job (del),
      A(AA), B(BB), OA(OOA), OB(OOB), L(LL),
      rStart (0), rCount(rCount_), rLength (cCount_),
      cStart (0), cCount(cCount_), cLength (rCount_),
      sizes_ptr(sizes_ptr_),
      sizes(NULL), node_id(0), level (0),
      stage(BT_ST_SIZE)
    {}

  BlockTranspose(E *AA, E *BB, int *OOA, int *OOB, int *LL,
		 int rStart_, int rCount_, int rLength_,
		 int cStart_, int cCount_, int cLength_,
		 int stage_,
		 lluint *sizes_=NULL, int node_id_=0, int level_=0,
		 bool del=true)
    : HR2Job (del),
      A(AA), B(BB), OA(OOA), OB(OOB), L(LL),
      rStart (rStart_), rCount(rCount_), rLength (rLength_),
      cStart (cStart_), cCount(cCount_), cLength (cLength_),
      sizes(sizes_), node_id(node_id_), level (level_),
      stage (stage_)
    {}

  // If you want to size block transpsose,
  // or just use it on a scheduler with out sizing, use this
  BlockTranspose (E *AA, E *BB, int *OOA, int *OOB, int *LL,
		  int rCount_, int cCount_,
		  int stage_,
		  lluint *sizes_=NULL,
		  bool del=true)
    : HR2Job (del),
      A(AA), B(BB), OA(OOA), OB(OOB), L(LL),
      rStart (0), rCount(rCount_), rLength (cCount_),
      cStart (0), cCount(cCount_), cLength (rCount_),
      sizes(sizes_), node_id(0), level (0),
      stage(stage_)
    {}
  
  // Stage 1 constructor
  // BlockTranspose (bool del=true)
  //: HR2Job (del),
  //  stage (1)
  //{}

  lluint size (const int block_size) {
    if ( stage == BT_ST_SIZE || stage == BT_ST_SIZE_JOIN) {
      return 2*(1+cCount/_PAR_TRANS_THRESHHOLD)*(1+rCount/_PAR_TRANS_THRESHHOLD)*sizeof(lluint)*1000;
    } else if (stage == 0 || stage == 1) {
      if (sizes==NULL) {
	std::cout<<"Block transpose needs to be pre-sized"<<std::endl;
	exit(-1);
      }
      //if (sizes[node_id] <= 0) { std::cerr<<sizes[node_id]<<std::endl; exit(-1);}
      return sizes[node_id];
    } else {
      std::cout<<"In function "<<__func__<<" invalid stage: "<<stage<<std::endl;
      exit(-1);
    }
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE == 1) {
      return size(block_size);
    } else {
      if (stage==0 && cCount<=_PAR_TRANS_THRESHHOLD && rCount<=_PAR_TRANS_THRESHHOLD) {
	return size(block_size);
      } else if (stage==BT_ST_SIZE && cCount<=_PAR_TRANS_THRESHHOLD && rCount<=_PAR_TRANS_THRESHHOLD) {
	return size(block_size);
      } else {
	return STRAND_SIZE;
      }
    }
  }

  void seq_blockTranspose(int rStart, int rCount, int rLength,
	      int cStart, int cCount, int cLength) {
    if (cCount < _TRANS_THRESHHOLD && rCount < _TRANS_THRESHHOLD) {
      for (int i=rStart; i < rStart + rCount; i++) 
	for (int j=cStart; j < cStart + cCount; j++) {
	  E* pa = A+OA[i*rLength + j];
	  E* pb = B+OB[j*cLength + i];
	  int l = L[i*rLength + j];
	  //cout << "pa,pb,l: " << pa << "," << pb << "," << l << endl;
	  for (int k=0; k < l; k++) *(pb++) = *(pa++);
	}
    } else if (cCount > rCount) {
      int l1 = cCount/2;
      int l2 = cCount - cCount/2;
      seq_blockTranspose (rStart,rCount,rLength,cStart,l1,cLength);
      seq_blockTranspose (rStart,rCount,rLength,cStart + l1,l2,cLength);
    } else {
      int l1 = rCount/2;
      int l2 = rCount - rCount/2;
      seq_blockTranspose (rStart,l1,rLength,cStart,cCount,cLength);
      seq_blockTranspose (rStart + l1,l2,rLength,cStart,cCount,cLength);
    }
  }
  
  void function () {
    //if (cCount*rCount > (1<<15))
    //std::cout<<cCount<<" "<<rCount<<" "<<stage<<" lvl:"<<level<<" stage: "<<stage<<std::endl;
    if (stage == 0) {
      //cout << "cc,rc: " << cCount << "," << rCount << endl;
      if (cCount < _PAR_TRANS_THRESHHOLD && rCount < _PAR_TRANS_THRESHHOLD) {
	seq_blockTranspose (rStart,rCount,rLength,cStart,cCount,cLength);
	join();
      } else if (cCount > rCount) {
	int l1 = cCount/2;
	int l2 = cCount - cCount/2;
	binary_fork (new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart,l1,cLength,
					    0, sizes+(1<<level), 2*node_id, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart + l1,l2,cLength,
					    0, sizes+(1<<level), 2*node_id+1, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart,cCount,cLength,
					    1, sizes, node_id, level));
      } else {
	int l1 = rCount/2;
	int l2 = rCount - rCount/2;
	binary_fork (new BlockTranspose<E> (A,B,OA,OB,L,rStart,l1,rLength,cStart,cCount,cLength,
					    0, sizes+(1<<level), 2*node_id, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart + l1,l2,rLength,cStart,cCount,cLength,
					    0, sizes+(1<<level), 2*node_id+1, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart,cCount,cLength,
					    1, sizes, node_id, level));
      }
    } else if (stage == 1) {
      join();
    } else if (stage == BT_ST_SIZE) {
      if (sizes == NULL) {
        sizes = new lluint [8*rCount*cCount/_PAR_TRANS_THRESHHOLD];
	*sizes_ptr = sizes;
	level = 0;
	node_id = 0;
      }

      if (cCount < _PAR_TRANS_THRESHHOLD && rCount < _PAR_TRANS_THRESHHOLD) {
	lluint sum=0;
	for (int i=rStart; i < rStart + rCount; i++) 
	  sum += (OA[i*rLength + cStart+cCount] - OA[i*rLength + cStart] > 0)
	    ? OA[i*rLength + cStart+cCount] - OA[i*rLength + cStart]
	    : 1;
	//std::cout<<sizes<< " "<<node_id<<std::endl;
	*(sizes+node_id) = 2*sum*sizeof(E) + rCount*cCount*sizeof(int); 
	join ();
      } else if (cCount > rCount) {
	int l1 = cCount/2;
	int l2 = cCount - cCount/2;
	binary_fork (new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart,l1,cLength,
					    BT_ST_SIZE, sizes+(1<<level), 2*node_id, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart + l1,l2,cLength,
					    BT_ST_SIZE, sizes+(1<<level), 2*node_id+1, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart,cCount,cLength,
					    BT_ST_SIZE_JOIN, sizes, node_id, level));
      } else {
	int l1 = rCount/2;
	int l2 = rCount - rCount/2;
	binary_fork (new BlockTranspose<E> (A,B,OA,OB,L,rStart,l1,rLength,cStart,cCount,cLength,
					    BT_ST_SIZE, sizes+(1<<level), 2*node_id, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart + l1,l2,rLength,cStart,cCount,cLength,
					    BT_ST_SIZE, sizes+(1<<level), 2*node_id+1, level+1),
		     new BlockTranspose<E> (A,B,OA,OB,L,rStart,rCount,rLength,cStart,cCount,cLength,
					    BT_ST_SIZE_JOIN, sizes, node_id, level));
      }
    } else if (stage == BT_ST_SIZE_JOIN) {
      *(sizes+node_id) = sizes[(1<<level)+2*node_id] + sizes[(1<<level)+2*node_id+1];
      //if (level<5)std::cout<<sizes[node_id]<<" "<<level<<" "<<node_id<<std::endl;
      join();
    } else {
      std::cerr << "In valid stage in function: "<<__func__<<std::endl;
      exit (-1);
    }
  }
};
  
/*
bool testBlockTranspose (int L, int BR) {
  int *Seg = new int[L*BR+1];
  int *OA = new int[L*BR+1];
  int *OB = new int [BR*L+1];
  
  for (int i=0; i<L*BR; ++i) {
    Seg[i] = rand()%4;
  }
  Seg[L*BR]=0;

  FIND_MACHINE;
  startTime();
  tp_init (num_procs, NULL, new HR_Scheduler(num_procs, num_levels, fan_outs, sizes, block_sizes),
	   //tp_init (num_procs, NULL, new WS_Scheduler(num_procs),
	   new Scan<int, plus<int> > (new int, Seg, OA, L*BR+1, plus<int>(), 0));
  tp_sync_all();
  nextTime ("Scan");
  tp_run (new Transpose<int> (Seg, OB, L, BR));
  tp_sync_all();
  nextTime ("Transpose");
  tp_run (new Scan<int, plus<int> > (new int, OB, OB, L*BR+1, plus<int>(), 0));
  tp_sync_all();
  nextTime ("Scan");
  
  double *A = new double[OA[L*BR-1]+Seg[L*BR-1]];
  double *B = new double[OA[L*BR-1]+Seg[L*BR-1]];
  for (int i=0; i<OA[L*BR-1]+Seg[L*BR-1]; ++i)
    A[i]=rand();

  startTime();
  lluint **bt_sizes_ptr;
  tp_run (new BlockTranspose<double> (A,B,OA,OB,Seg,L,BR,bt_sizes_ptr));
  tp_sync_all();
  nextTime ("Sizing Transpose");
  tp_run (new BlockTranspose<double> (A,B,OA,OB,Seg,L,BR,0,*bt_sizes_ptr));
  tp_sync_all();
  nextTime ("Done Block Transpose");
  
  int good=true;
  for (int i=0; i<L; ++i)
    for (int j=0; j<BR; ++j)
      for (int k=0; k<Seg[i*BR+j]; ++k)
	if (A[OA[i*BR+j] + k] != B[OB[j*L+i]+k]) {
	  std::cout<<"BTranpose bad at i: "<<i<<",j: "<<j<<",k: "<<k<<std::endl;
	  delete A; delete B; delete Seg; delete OA; delete OB;
	  return false;
	}
  delete A; delete B; delete Seg; delete OA; delete OB;
  std::cout<<"Block Transpose Good"<<std::endl;
  return true;
}
*/

/*
template <class E>
struct transpose {
  E *A, *B;
  transpose(E *AA, E *BB) : A(AA), B(BB) {}

  void transR(int rStart, int rCount, int rLength,
	      int cStart, int cCount, int cLength) {
    //cout << "cc,rc: " << cCount << "," << rCount << endl;
    if (cCount < _TRANS_THRESHHOLD && rCount < _TRANS_THRESHHOLD) {
      for (int i=rStart; i < rStart + rCount; i++) 
	for (int j=cStart; j < cStart + cCount; j++) 
	  B[j*cLength + i] = A[i*rLength + j];
    } else if (cCount > rCount) {
      int l1 = cCount/2;
      int l2 = cCount - cCount/2;
      cilk_spawn transR(rStart,rCount,rLength,cStart,l1,cLength);
      transR(rStart,rCount,rLength,cStart + l1,l2,cLength);
      cilk_sync;
    } else {
      int l1 = rCount/2;
      int l2 = rCount - rCount/2;
      cilk_spawn transR(rStart,l1,rLength,cStart,cCount,cLength);
      transR(rStart + l1,l2,rLength,cStart,cCount,cLength);
      cilk_sync;
    }	
  }

  void trans(int rCount, int cCount) {
    transR(0,rCount,cCount,0,cCount,rCount);
  }
};

template <class E>
struct blockTrans {
  E *A, *B;
  int *OA, *OB, *L;

  blockTrans(E *AA, E *BB, int *OOA, int *OOB, int *LL) 
    : A(AA), B(BB), OA(OOA), OB(OOB), L(LL) {}

  void transR(int rStart, int rCount, int rLength,
	     int cStart, int cCount, int cLength) {
    //cout << "cc,rc: " << cCount << "," << rCount << endl;
    if (cCount < _TRANS_THRESHHOLD && rCount < _TRANS_THRESHHOLD) {
      for (int i=rStart; i < rStart + rCount; i++) 
	for (int j=cStart; j < cStart + cCount; j++) {
	  E* pa = A+OA[i*rLength + j];
	  E* pb = B+OB[j*cLength + i];
	  int l = L[i*rLength + j];
	  //cout << "pa,pb,l: " << pa << "," << pb << "," << l << endl;
	  for (int k=0; k < l; k++) *(pb++) = *(pa++);
	}
    } else if (cCount > rCount) {
      int l1 = cCount/2;
      int l2 = cCount - cCount/2;
      cilk_spawn transR(rStart,rCount,rLength,cStart,l1,cLength);
      transR(rStart,rCount,rLength,cStart + l1,l2,cLength);
      cilk_sync;
    } else {
      int l1 = rCount/2;
      int l2 = rCount - rCount/2;
      cilk_spawn transR(rStart,l1,rLength,cStart,cCount,cLength);
      transR(rStart + l1,l2,rLength,cStart,cCount,cLength);
      cilk_sync;
    }	
  }
 
  void trans(int rCount, int cCount) {
    transR(0,rCount,cCount,0,cCount,rCount);
  }

} ;
*/
#endif // A_TRANSPOSE_INCLUDED
