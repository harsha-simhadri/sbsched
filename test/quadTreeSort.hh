// This code is part of the project "Experimental Analysis of Space-Bounded
// Schedulers", presented at Symposium on Parallelism in Algorithms and
// Architectures, 2014.
// Copyright (c) 2014 Harsha Vardhan Simhadri, Guy Blelloch, Phillip Gibbons,
// Jeremy Fineman, Aapo Kyrola.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef __QUADTREESORT_HH
#define __QUADTREESORT_HH

#include <algorithm>
#include <functional>
#include "sequence-jobs.hh"
#include "Job.hh"
#include "utils.hh"

using namespace std;


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
class CompareWithCenter {
  const Point<E> center;  
public:
  CompareWithCenter (Point<E> center_) : center(center_) {}
  inline int operator() (const Point<E> pt) const {
    if (pt.x < center.x) {
      if (pt.y < center.y) 
	return TOPLEFT;
      else
	return TOPRIGHT;
    } else { //pt.x >= center.x
      if (pt.y < center.y) 
	return BOTTOMLEFT;
      else
	return BOTTOMRIGHT;
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
#define QSORT_PAR_THRESHOLD (1<<16)

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

template <class E>
class QuadTreeSort : public HR2Job {
  Point<E> *A, *B; int n;
  Box<E> bx;

  int *compared;
  int *tl_pos, *tr_pos, *bl_pos, *br_pos;

  // not sure why these are pointers, but I will copy for now
  int tl_n, tr_n, bl_n, br_n;
  
  bool invert;
  int stage;
  
public:
  QuadTreeSort (Point<E>* A_, Box<E> bx_, int n_, 
		int* compared_=NULL, int* tl_pos_=NULL, int* tr_pos_=NULL, int* bl_pos_=NULL, int* br_pos_=NULL,
		Point<E>* B_=NULL,
		int stage_=0, bool invert_=0,bool del=true)
    : HR2Job (del), A(A_), bx(bx_), n(n_), 
      compared(compared_), tl_pos(tl_pos_), tr_pos(tr_pos_), bl_pos(bl_pos_), br_pos(br_pos_),
      B(B_), 
      invert(invert_), stage(stage_) {}

  QuadTreeSort (QuadTreeSort<E> *from, bool del=true)
    : HR2Job (del), A(from->A), B(from->B), bx(from->bx), n(from->n), 
      compared(from->compared), tl_pos(from->tl_pos), tr_pos(from->tr_pos), bl_pos(from->bl_pos), br_pos(from->br_pos),
      tl_n(from->tl_n), tr_n(from->tr_n), bl_n(from->bl_n), br_n(from->br_n),  
      invert(from->invert),stage(from->stage + 1) {}

  lluint size (const int block_size) {
    return quadSortSize<E>(n,block_size);
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      if (stage==0 && n<QSORT_PAR_THRESHOLD) {
	return size(block_size);
      } else {
	return STRAND_SIZE;
      }
    }
  }

  inline QuadTreeSort<E>* nextStage() {stage++; return this;}
    
  void function () {
    _delete = false;
    if (stage == 0) {
      if (n < QSORT_PAR_THRESHOLD || bx.isSmall()) {
	_delete = true;
	seqQuadSort<E>(A,bx,n,B,invert);
	join ();
      } else {
	assert(compared!=NULL && tl_pos!=NULL && tr_pos!=NULL && bl_pos!=NULL && br_pos!=NULL && B!=NULL);
      
	unary_fork (new Map<Point<E>,int,CompareWithCenter<E> > (A,compared,n, CompareWithCenter<E>(bx.center())  ),
		    nextStage());
      }
    } else if (stage == 1) {
	PositionScanPlus tlScanPlus(TOPLEFT);

	unary_fork (new Scan<int,PositionScanPlus> (&tl_n,compared,tl_pos,n,tlScanPlus,0),
		    nextStage());

    } else if (stage == 2) {
	PositionScanPlus trScanPlus(TOPRIGHT);

	unary_fork (new Scan<int,PositionScanPlus> (&tr_n,compared,tr_pos,n,trScanPlus,0),
		    nextStage());

    } else if (stage == 3) {
	PositionScanPlus blScanPlus(BOTTOMLEFT);

	unary_fork (new Scan<int,PositionScanPlus> (&bl_n,compared,bl_pos,n,blScanPlus,0),
		    nextStage());

    } else if (stage == 4) {
	PositionScanPlus brScanPlus(BOTTOMRIGHT);

	unary_fork (new Scan<int,PositionScanPlus> (&br_n,compared,br_pos,n,brScanPlus,0),
		    nextStage());
	
    } else if (stage == 5) {

      unary_fork (new FilterQuadrants<E> (A,B,B+tl_n,B+tl_n+tr_n, B+tl_n+tr_n+bl_n,
				       tl_pos, tr_pos, bl_pos, br_pos,
				       compared,0,n),
		  nextStage());

    } else if (stage == 6) {
      Job **children = new Job*[4];
      
      int os = 0;
      children[0] = new QuadTreeSort<E>(B, bx.quadrant(TOPLEFT), tl_n, compared, 
					tl_pos, tr_pos, bl_pos, br_pos,
					A,0,!invert);
      
      os = tl_n;
      children[1] = new QuadTreeSort<E>(B+os, bx.quadrant(TOPRIGHT), tr_n, compared+os,
					tl_pos+os, tr_pos+os, bl_pos+os, br_pos+os,
					A+os,0,!invert);

      os = tl_n + tr_n;
      children[2] = new QuadTreeSort<E>(B+os, bx.quadrant(BOTTOMLEFT), bl_n, compared+os,
					tl_pos+os, tr_pos+os, bl_pos+os, br_pos+os,
					A+os,0,!invert);
      
      os = tl_n + tr_n + bl_n;
      children[3] = new QuadTreeSort<E>(B+os, bx.quadrant(BOTTOMRIGHT), br_n, compared+os,
					tl_pos+os, tr_pos+os, bl_pos+os, br_pos+os,
					A+os,0,!invert);
      
      fork(4,
	   children,
	   nextStage());

    } else if (stage == 7) {
      // JEREMY: There was a case for invert here before in quicksort
      // I think it was only handling the nonrecursively sorted equal elements
      // this doesn't apply here since I recursively sort all.
      _delete = true;
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
