#ifndef __QUADTREESORT_HH
#define __QUADTREESORT_HH

#include <algorithm>
#include <functional>
#include "sequence-jobs.hh"
#include "Job.hh"
#include "utils.hh"
using namespace std;
#include "collect.hh"


long min(long a, long b) {
  if (a<b) return a;
  else return b;
}

/** 
 * (0,0) is the top left corner, and the coordinates increase going
 * down and right.  
 *
 * For some reason I also reversed the coordinates though, thinking of
 * them as (row,column), so increasing the "x" coordinate actually
 * means move down. As long as the code is consistent, it shouldn't matter.
 * 
 * The sorted order is TopLeft, TopRight, BottomLeft, BottomRight
 */
#define SAME 0
#define TOPLEFT -1
#define TOPRIGHT -2
#define BOTTOMLEFT -3
#define BOTTOMRIGHT -4

//Smallest box dimension allowed
#define PRECISION 1

template <class E>
class Point {
public:
  E x, y;

  Point() { x=0; y=0; }
  Point(E x_, E y_) : x(x_), y(y_) {}
  Point(const Point<E>& p) : x(p.x), y(p.y) {}
};

template <class E>
class Box {
public:
  Point<E> tl, br;
  
  Box() {}
  Box(Point<E> tl_, Point<E> br_) : tl(tl_), br(br_) {}
  Box(E min, E max) : tl(min,min), br(max,max) {}
  Box(const Box<E>& b) : tl(b.tl), br(b.br) {}

  //Construct the proper quadrant of the bounding box
  Box(const Box<E>& b, int quad) {
    E midx = (b.tl.x + b.br.x)/2;
    E midy = (b.tl.y + b.br.y)/2;

    switch (quad) {
    case SAME:
      tl.x = b.tl.x;
      tl.y = b.tl.y;
      br.x = b.br.x;
      br.y = b.br.y;
      return; 
    case TOPLEFT:
      tl.x = b.tl.x;
      tl.y = b.tl.y;
      br.x = midx;
      br.y = midy;
      return;
    case TOPRIGHT:
      tl.x = b.tl.x;
      tl.y = midy;
      br.x = midx;
      br.y = b.br.y;
      return;
    case BOTTOMLEFT:
      tl.x = midx;
      tl.y = b.tl.y;
      br.x = b.br.x;
      br.y = midy;
      return;
    case BOTTOMRIGHT:
      tl.x = midx;
      tl.y = midy;
      br.x = b.br.x;
      br.y = b.br.y;
      return;
    }
  }

  Point<E> center() {
    Point<E> center( (tl.x+br.x)/2, (tl.y+br.y)/2 );
    return center;
  }

  Box<E> quadrant(int quad) {
    Box<E> newbox(*this, quad);
    return newbox;
  }

  inline bool isSmall() {return ( ((br.x-tl.x)<=PRECISION) || ((br.y-tl.y)<=PRECISION));}
};

template <class E>  
class Quadrant {
  Point<E>* A;
  const Point<E> center;  
public:
  Quadrant (Point<E>* A_, Point<E> center_) : 
    A(A_), center(center_) {}

  inline int operator() (int i) {
    Point<E> pt = A[i];
    if (pt.x < center.x) {
      if (pt.y < center.y) 
	return 0;
      else
	return 1;
    } else { //pt.x >= center.x
      if (pt.y < center.y) 
	return 2;
      else
	return 3;
    }
  }
};

class PositionScanPlus {
  const int selector;
public:
  PositionScanPlus () : selector(0) {}  // DO NOT USE. JUST TO KEEP COMPILER HAPPY
  PositionScanPlus (int selector_) : selector(selector_) {}
  inline int operator() (const int& x, const int&y) {
    int x_plus=x; int y_plus=y;
    if (x<0) x_plus = x==selector ? 1 : 0;
    if (y<0) y_plus = y==selector ? 1 : 0;
    return x_plus + y_plus;
  }
};


template <class E>
class FilterQuadrants : public HR2Job {

  Point<E> *In,*tl,*tr,*bl,*br;
  int *tl_pos, *tr_pos, *bl_pos, *br_pos;
  int *compared;  int n,finger;
  int stage; 

public:
  FilterQuadrants (Point<E> *In_,
		   Point<E>* tl_, Point<E>* tr_, Point<E>* bl_, Point<E>* br_,
		   int* tl_pos_, int* tr_pos_, int* bl_pos_, int* br_pos_,
		   int* compared_,
		   int finger_, int n_,  int stage_=0, 
		   bool del=true)
    : HR2Job (del),
      In(In_),tl(tl_),tr(tr_),bl(bl_),br(br_),
      tl_pos(tl_pos_),tr_pos(tr_pos_),bl_pos(bl_pos_),br_pos(br_pos_),
      compared(compared_),
      finger(finger_),n(n_),stage(stage_)  {}

  lluint size (const int block_size) {
    return 2*round_up((n+1)*sizeof(Point<E>), block_size)+5*round_up((n+1)*sizeof(int), block_size);
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) 
      return size (block_size); 
    else 
      if (stage == 0 && n<_SCAN_BSIZE) 
	return size(block_size);
      else 
	return STRAND_SIZE;
  }

  inline FilterQuadrants<E>* nextStage() {stage++; return this;}
  
  void function () {
    _delete = false;
    if (stage == 0) {
      if (n<_SCAN_BSIZE) {
	_delete = true;
	for (int i=0; i<n; ++i)
	  if (compared[finger+i]==TOPLEFT)
	    *(tl++)=*(In++);
	  else if (compared[finger+i]==TOPRIGHT)
	    *(tr++)=*(In++);
	  else if (compared[finger+i]==BOTTOMLEFT)
	    *(bl++)=*(In++);
	  else // BOTTOMRIGHT
	    *(br++)=*(In++);	
	join ();
      } else {
	int mid = finger+n/2;
	int tl_less = tl_pos[mid]-tl_pos[finger];
	int tr_less = tr_pos[mid]-tr_pos[finger];
	int bl_less = bl_pos[mid]-bl_pos[finger];
	int br_less = br_pos[mid]-br_pos[finger];

	binary_fork (new FilterQuadrants<E>(In,tl,tr,bl,br,
					 tl_pos,tr_pos,bl_pos,br_pos,
					 compared,finger,n/2,0),
		     new FilterQuadrants<E>(In+n/2,tl+tl_less,tr+tr_less,bl+bl_less,br+br_less,
					 tl_pos,tr_pos,bl_pos,br_pos,
					 compared,mid,n-n/2,0),
		     nextStage());
      }
    } else {
      _delete = true;
      join ();
    }
  }
};

#define ISORT 32
#define QSORT_PAR_THRESHOLD (1<<14)

// do an inplace partition around x coordinate of center in A
// return the number of elements in the left
template <class E>
int seqFilterX(Point<E>* A, E xcoord, int n) {
  Point<E>* top = A;
  Point<E>* bottom = A+n-1;
  while (1) {
    // everything in A[0..top-1] belongs in top
    // everything in A[bottom+1..n-1] belongs in bottom
    while ((top <= bottom) && ((*top).x < xcoord)) top++;
    while ((bottom >= top) && ((*bottom).x >= xcoord)) bottom--;
    if (top >= bottom) break;
    swap(*top,*bottom);
  } 
  return top-A;
}

template <class E>
int seqFilterY(Point<E>* A, E ycoord, int n) {
  Point<E>* left = A;
  Point<E>* right = A+n-1;
  while (1) {
    while ((left <= right) && ((*left).y < ycoord)) left++;
    while ((right >= left) && ((*right).y >= ycoord)) right--;
    if (left >= right) break;
    swap(*left,*right);
  }
  return left-A;
}

// If invert = 1, then A is sorted into B
template <class E>
void seqQuadSort(Point<E>* A, Box<E> bx, int n, Point<E>* B=NULL, bool invert=0) {
  /*
  if (n < ISORT) insertionSort(A, n, f);
  else {
  */
  
  if (n <= 1 || bx.isSmall()) {
    // already sorted. there should be some other base case?
  } else {
    Point<E> center = bx.center();
    // Sort by 3 passes: first do top and bottom, then left and right
    int nTop = seqFilterX(A,center.x,n);
    int nTopLeft = seqFilterY(A,center.y,nTop);
    int nBottomLeft = seqFilterY(A+nTop,center.y,n-nTop);

    seqQuadSort<E>(A, bx.quadrant(TOPLEFT),nTopLeft);
    seqQuadSort<E>(A+nTopLeft,bx.quadrant(TOPRIGHT),nTop-nTopLeft);
    seqQuadSort<E>(A+nTop,bx.quadrant(BOTTOMLEFT),nBottomLeft);
    seqQuadSort<E>(A+nTop+nBottomLeft,bx.quadrant(BOTTOMRIGHT),n-nTop-nBottomLeft);
  }

  if (invert==1) {
    for (int i=0;i<n;++i)
      B[i]=A[i];
  }
}

template <class E>
lluint quadSortSize (int n, int block_size) {
  return 2*((lluint)(n+1))*sizeof(Point<E>) 
    + 5*((lluint)(n+1))*sizeof(int) // compared array + 4 quadrant sum arrays
    + 5*block_size;
}

typedef char btype;

template <class E>
class QuadTreeSort : public HR2Job {
  Point<E> *A, *B; int n;
  Box<E> bx;
  int* counts;
  int numBlocks;
  btype* bucketId;
  int* bucketStarts;
  bool invert;
  int stage;
  
public:
  QuadTreeSort (Point<E>* A_, Box<E> bx_, int n_, Point<E>* B_,
		btype* bucketId_, bool invert_=0, 
		int stage_=0, bool del=true)
    : HR2Job (del), A(A_), bx(bx_), n(n_), B(B_), 
      bucketId(bucketId_), invert(invert_), stage(stage_) {}

  QuadTreeSort (QuadTreeSort<E> *from, bool del=true)
    : HR2Job (del), A(from->A), B(from->B), bx(from->bx), n(from->n), 
      numBlocks(from->numBlocks), bucketId(from->bucketId),
      counts(from->counts), bucketStarts(from->bucketStarts),
      invert(from->invert), stage(from->stage + 1) {}

  lluint size (const int block_size) {
    return n*(2*sizeof(Point<E>)+sizeof(btype));
  }

  lluint strand_size (const int block_size) { return size(block_size); }

  //inline QuadTreeSort<E>* nextStage() {stage++; return this;}
    
  void function () {
    //_delete = false;
    if (stage == 0) {
      //if (n < 40000000) {  join();  } else
      if (n < QSORT_PAR_THRESHOLD || bx.isSmall()) {
	//_delete = true;
	seqQuadSort<E>(A,bx,n,B,invert);
	join ();
      } else {
	bucketStarts = newA(int, 4+1);
	unary_fork(new Collect<Point<E>,btype,Quadrant<E> >
		   (A, B, n, 4, bucketId, bucketStarts,
		    Quadrant<E>(A, bx.center())),
		   new QuadTreeSort(this));
      }
	
    } else if (stage == 1) {
      Job **children = new Job*[4];
      for (int i=0; i < 4; i++) {
	Point<E> *B_ = B + bucketStarts[i];
	Point<E> *A_ = A + bucketStarts[i];
	btype *bid_ = bucketId + bucketStarts[i];
	int n_ = bucketStarts[i+1]-bucketStarts[i];
	children[i] = new QuadTreeSort<E>(B_, bx.quadrant(-(i+1)), n_, 
					  A_, bid_, !invert);
      }
      free(bucketStarts);
      fork(4, children, new QuadTreeSort(this));
	   //nextStage());
    } else if (stage == 2) {
	//_delete = true;
      join();
    }
  }
};


//Check is only implemented for integers
template <class E>
bool checkSort (Point<E>* A, int n) {
  std::cout<<"WARNING: The check is only implemented for integers!" << std::endl;
  return true;
}


// true if a <= b in morton order. only correct for bounding box at powers of 2
bool
comparePoints(Point<int> a, Point<int> b) {
  int bits = sizeof(int)*8;
  for (int shift = bits-1; shift >=0; shift--) {
    if ((a.x >> shift) < (b.x >> shift)) {
      return true;
    } else if ((a.x >> shift) > (b.x >> shift)) {
      return false;
    }

    if ((a.y >> shift) < (b.y >> shift)) {
      return true;
    } else if ((a.y >> shift) > (b.y >> shift)) {
      return false;
    }
  }
  return true;
}
    
//Only correct for bounding box at powers of 2 
template <>
bool checkSort<int> (Point<int>* A, int n) {
  std::cout<<"doing the integer check"<<std::endl;

  for (int i=0; i<n-1; ++i) {
    if (!comparePoints(A[i],A[i+1])) {
      std::cout<<"Sort check failed at: "<<i<<std::endl;
      std::cout<<"("<<A[i].x<<","<<A[i].y<<") and ("<<A[i+1].x<<","<<A[i+1].y<<")"<<std::endl;
      return false;
    }
  }
  /*
  for (int i=0; i<n-1; ++i) 
    if (!f(A[i], A[i+1])) {
      std::cout<<"Sort check failed at: "<<i<<std::endl;
      return false;
    }
  */
  return true;
}

#endif
