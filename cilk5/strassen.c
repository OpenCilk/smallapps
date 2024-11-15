/*
 * Copyright (c) 1996 Massachusetts Institute of Technology
 * Copyright (c) 2024 Tao B. Schardl
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to use, copy, modify, and distribute the Software without
 * restriction, provided the Software, including any modified copies made
 * under this license, is not distributed for a fee, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE MASSACHUSETTS INSTITUTE OF TECHNOLOGY BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the Massachusetts
 * Institute of Technology shall not be used in advertising or otherwise
 * to promote the sale, use or other dealings in this Software without
 * prior written authorization from the Massachusetts Institute of
 * Technology.
 *
 */

#include "getoptions.h"
#include <cilk/cilk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef SERIAL
#include <cilk/cilk_stub.h>
#endif

unsigned long long todval(struct timeval *tp) {
  return tp->tv_sec * 1000 * 1000 + tp->tv_usec;
}

#ifndef RAND_MAX
#define RAND_MAX 32767
#endif

#define SizeAtWhichDivideAndConquerIsMoreEfficient 64
#define SizeAtWhichNaiveAlgorithmIsMoreEfficient 16
#define CacheBlockSizeInBytes 32

/* The real numbers we are using --- either double or float */
typedef double REAL;
typedef unsigned long PTR;

/* maximum tolerable relative error (for the checking routine) */
#define EPSILON (1.0E-6)

/*
 * Matrices are stored in row-major order; A is a pointer to
 * the first element of the matrix, and an is the number of elements
 * between two rows. This macro produces the element A[i,j]
 * given A, an, i and j
 */
#define ELEM(A, an, i, j) (A[(i) * (an) + (j)])

unsigned long rand_nxt = 0;

int cilk_rand(void) {
  rand_nxt = rand_nxt * 1103515245 + 12345;
  int result = (rand_nxt >> 16) % ((unsigned int)RAND_MAX + 1);
  return result;
}

/*
 * ANGE:
 * recursively multiply an m x n matrix A with size n vector V, and store
 * result in vector size m P.  The value rw is the row width of A, and
 * add the result into P if variable add != 0
 */
void mat_vec_mul(int m, int n, int rw, REAL *A, REAL *V, REAL *P, int add) {

  if ((m + n) <= 64) { // base case

    if (add) {
      for (int i = 0; i < m; i++) {
        REAL c = 0;
        for (int j = 0; j < n; j++) {
          c += ELEM(A, rw, i, j) * V[j];
        }
        P[i] += c;
      }
    } else {
      for (int i = 0; i < m; i++) {
        REAL c = 0;
        for (int j = 0; j < n; j++) {
          c += ELEM(A, rw, i, j) * V[j];
        }
        P[i] = c;
      }
    }

  } else if (m >= n) { // cut m dimension
    int m1 = m >> 1;
    mat_vec_mul(m1, n, rw, A, V, P, add);
    mat_vec_mul(m - m1, n, rw, &ELEM(A, rw, m1, 0), V, P + m1, add);

  } else { // cut n dimension
    int n1 = n >> 1;
    mat_vec_mul(m, n1, rw, A, V, P, add);
    // sync here if parallelized
    mat_vec_mul(m, n - n1, rw, &ELEM(A, rw, 0, n1), V + n1, P, 1);
  }
}

/*
 * Naive sequential algorithm, for comparison purposes
 */
void matrixmul(int n, REAL *A, int an, REAL *B, int bn, REAL *C, int cn) {

  for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j) {
      REAL s = 0.0;
      for (int k = 0; k < n; ++k)
        s += ELEM(A, an, i, k) * ELEM(B, bn, k, j);

      ELEM(C, cn, i, j) = s;
    }
}

/*****************************************************************************
**
** FastNaiveMatrixMultiply
**
** For small to medium sized matrices A, B, and C of size
** MatrixSize * MatrixSize this function performs the operation
** C = A x B efficiently.
**
** Note MatrixSize must be divisible by 8.
**
** INPUT:
**    C = (*C WRITE) Address of top left element of matrix C.
**    A = (*A IS READ ONLY) Address of top left element of matrix A.
**    B = (*B IS READ ONLY) Address of top left element of matrix B.
**    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
**    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
**    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
**    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
**
** OUTPUT:
**    C = (*C WRITE) Matrix C contains A x B. (Initial value of *C undefined.)
**
*****************************************************************************/
void FastNaiveMatrixMultiply(REAL *C, REAL *A, REAL *B, unsigned MatrixSize,
                             unsigned RowWidthC, unsigned RowWidthA,
                             unsigned RowWidthB) {

  /* Assumes size of real is 8 bytes */
  PTR RowWidthBInBytes = RowWidthB << 3;
  PTR RowWidthAInBytes = RowWidthA << 3;
  PTR MatrixWidthInBytes = MatrixSize << 3;
  PTR RowIncrementC = (RowWidthC - MatrixSize) << 3;
  unsigned Horizontal, Vertical;
#ifdef DEBUG_ON
  REAL *OLDC = C;
  REAL *TEMPMATRIX;
#endif

  REAL *ARowStart = A;
  for (Vertical = 0; Vertical < MatrixSize; Vertical++) {
    for (Horizontal = 0; Horizontal < MatrixSize; Horizontal += 8) {
      REAL *BColumnStart = B + Horizontal;
      REAL FirstARowValue = *ARowStart++;

      REAL Sum0 = FirstARowValue * (*BColumnStart);
      REAL Sum1 = FirstARowValue * (*(BColumnStart + 1));
      REAL Sum2 = FirstARowValue * (*(BColumnStart + 2));
      REAL Sum3 = FirstARowValue * (*(BColumnStart + 3));
      REAL Sum4 = FirstARowValue * (*(BColumnStart + 4));
      REAL Sum5 = FirstARowValue * (*(BColumnStart + 5));
      REAL Sum6 = FirstARowValue * (*(BColumnStart + 6));
      REAL Sum7 = FirstARowValue * (*(BColumnStart + 7));

      unsigned Products;
      for (Products = 1; Products < MatrixSize; Products++) {
        REAL ARowValue = *ARowStart++;
        BColumnStart = (REAL *)(((PTR)BColumnStart) + RowWidthBInBytes);

        Sum0 += ARowValue * (*BColumnStart);
        Sum1 += ARowValue * (*(BColumnStart + 1));
        Sum2 += ARowValue * (*(BColumnStart + 2));
        Sum3 += ARowValue * (*(BColumnStart + 3));
        Sum4 += ARowValue * (*(BColumnStart + 4));
        Sum5 += ARowValue * (*(BColumnStart + 5));
        Sum6 += ARowValue * (*(BColumnStart + 6));
        Sum7 += ARowValue * (*(BColumnStart + 7));
      }
      ARowStart = (REAL *)(((PTR)ARowStart) - MatrixWidthInBytes);

      *(C) = Sum0;
      *(C + 1) = Sum1;
      *(C + 2) = Sum2;
      *(C + 3) = Sum3;
      *(C + 4) = Sum4;
      *(C + 5) = Sum5;
      *(C + 6) = Sum6;
      *(C + 7) = Sum7;
      C += 8;
    }

    ARowStart = (REAL *)(((PTR)ARowStart) + RowWidthAInBytes);
    C = (REAL *)(((PTR)C) + RowIncrementC);
  }
}

/*****************************************************************************
 **
 ** FastAdditiveNaiveMatrixMultiply
 **
 ** For small to medium sized matrices A, B, and C of size
 ** MatrixSize * MatrixSize this function performs the operation
 ** C += A x B efficiently.
 **
 ** Note MatrixSize must be divisible by 8.
 **
 ** INPUT:
 **    C = (*C READ/WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **
 ** OUTPUT:
 **    C = (*C READ/WRITE) Matrix C contains C + A x B.
 **
 *****************************************************************************/
void FastAdditiveNaiveMatrixMultiply(REAL *C, REAL *A, REAL *B,
                                     unsigned MatrixSize, unsigned RowWidthC,
                                     unsigned RowWidthA, unsigned RowWidthB) {

  /* Assumes size of real is 8 bytes */
  PTR RowWidthBInBytes = RowWidthB << 3;
  PTR RowWidthAInBytes = RowWidthA << 3;
  PTR MatrixWidthInBytes = MatrixSize << 3;
  PTR RowIncrementC = (RowWidthC - MatrixSize) << 3;

  REAL *ARowStart = A;
  for (unsigned Vertical = 0; Vertical < MatrixSize; Vertical++) {
    for (unsigned Horizontal = 0; Horizontal < MatrixSize; Horizontal += 8) {
      REAL *BColumnStart = B + Horizontal;

      REAL Sum0 = *C;
      REAL Sum1 = *(C + 1);
      REAL Sum2 = *(C + 2);
      REAL Sum3 = *(C + 3);
      REAL Sum4 = *(C + 4);
      REAL Sum5 = *(C + 5);
      REAL Sum6 = *(C + 6);
      REAL Sum7 = *(C + 7);

      unsigned Products;
      for (Products = 0; Products < MatrixSize; Products++) {
        REAL ARowValue = *ARowStart++;

        Sum0 += ARowValue * (*BColumnStart);
        Sum1 += ARowValue * (*(BColumnStart + 1));
        Sum2 += ARowValue * (*(BColumnStart + 2));
        Sum3 += ARowValue * (*(BColumnStart + 3));
        Sum4 += ARowValue * (*(BColumnStart + 4));
        Sum5 += ARowValue * (*(BColumnStart + 5));
        Sum6 += ARowValue * (*(BColumnStart + 6));
        Sum7 += ARowValue * (*(BColumnStart + 7));

        BColumnStart = (REAL *)(((PTR)BColumnStart) + RowWidthBInBytes);
      }
      ARowStart = (REAL *)(((PTR)ARowStart) - MatrixWidthInBytes);

      *(C) = Sum0;
      *(C + 1) = Sum1;
      *(C + 2) = Sum2;
      *(C + 3) = Sum3;
      *(C + 4) = Sum4;
      *(C + 5) = Sum5;
      *(C + 6) = Sum6;
      *(C + 7) = Sum7;
      C += 8;
    }

    ARowStart = (REAL *)(((PTR)ARowStart) + RowWidthAInBytes);
    C = (REAL *)(((PTR)C) + RowIncrementC);
  }
}

/*****************************************************************************
 **
 ** MultiplyByDivideAndConquer
 **
 ** For medium to medium-large (would you like fries with that) sized
 ** matrices A, B, and C of size MatrixSize * MatrixSize this function
 ** efficiently performs the operation
 **    C  = A x B (if AdditiveMode == 0)
 **    C += A x B (if AdditiveMode != 0)
 **
 ** Note MatrixSize must be divisible by 16.
 **
 ** INPUT:
 **    C = (*C READ/WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **    AdditiveMode = 0 if we want C = A x B, otherwise we'll do C += A x B
 **
 ** OUTPUT:
 **    C (+)= A x B. (+ if AdditiveMode != 0)
 **
 *****************************************************************************/
void MultiplyByDivideAndConquer(REAL *C, REAL *A, REAL *B, unsigned MatrixSize,
                                unsigned RowWidthC, unsigned RowWidthA,
                                unsigned RowWidthB, int AdditiveMode) {

#define A00 A
#define B00 B
#define C00 C

  unsigned QuadrantSize = MatrixSize >> 1;

  /* partition the matrix */
  REAL *A01 = A00 + QuadrantSize;
  REAL *A10 = A00 + RowWidthA * QuadrantSize;
  REAL *A11 = A10 + QuadrantSize;

  REAL *B01 = B00 + QuadrantSize;
  REAL *B10 = B00 + RowWidthB * QuadrantSize;
  REAL *B11 = B10 + QuadrantSize;

  REAL *C01 = C00 + QuadrantSize;
  REAL *C10 = C00 + RowWidthC * QuadrantSize;
  REAL *C11 = C10 + QuadrantSize;

  if (QuadrantSize > SizeAtWhichNaiveAlgorithmIsMoreEfficient) {

    MultiplyByDivideAndConquer(C00, A00, B00, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C01, A00, B01, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C11, A10, B01, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C10, A10, B00, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, AdditiveMode);

    MultiplyByDivideAndConquer(C00, A01, B10, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, 1);

    MultiplyByDivideAndConquer(C01, A01, B11, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, 1);

    MultiplyByDivideAndConquer(C11, A11, B11, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, 1);

    MultiplyByDivideAndConquer(C10, A11, B10, QuadrantSize, RowWidthC,
                               RowWidthA, RowWidthB, 1);

  } else {

    if (AdditiveMode) {
      FastAdditiveNaiveMatrixMultiply(C00, A00, B00, QuadrantSize, RowWidthC,
                                      RowWidthA, RowWidthB);

      FastAdditiveNaiveMatrixMultiply(C01, A00, B01, QuadrantSize, RowWidthC,
                                      RowWidthA, RowWidthB);

      FastAdditiveNaiveMatrixMultiply(C11, A10, B01, QuadrantSize, RowWidthC,
                                      RowWidthA, RowWidthB);

      FastAdditiveNaiveMatrixMultiply(C10, A10, B00, QuadrantSize, RowWidthC,
                                      RowWidthA, RowWidthB);

    } else {

      FastNaiveMatrixMultiply(C00, A00, B00, QuadrantSize, RowWidthC, RowWidthA,
                              RowWidthB);

      FastNaiveMatrixMultiply(C01, A00, B01, QuadrantSize, RowWidthC, RowWidthA,
                              RowWidthB);

      FastNaiveMatrixMultiply(C11, A10, B01, QuadrantSize, RowWidthC, RowWidthA,
                              RowWidthB);

      FastNaiveMatrixMultiply(C10, A10, B00, QuadrantSize, RowWidthC, RowWidthA,
                              RowWidthB);
    }

    FastAdditiveNaiveMatrixMultiply(C00, A01, B10, QuadrantSize, RowWidthC,
                                    RowWidthA, RowWidthB);

    FastAdditiveNaiveMatrixMultiply(C01, A01, B11, QuadrantSize, RowWidthC,
                                    RowWidthA, RowWidthB);

    FastAdditiveNaiveMatrixMultiply(C11, A11, B11, QuadrantSize, RowWidthC,
                                    RowWidthA, RowWidthB);

    FastAdditiveNaiveMatrixMultiply(C10, A11, B10, QuadrantSize, RowWidthC,
                                    RowWidthA, RowWidthB);
  }

  return;
}

/*****************************************************************************
 **
 ** OptimizedStrassenMultiply
 **
 ** For large matrices A, B, and C of size MatrixSize * MatrixSize this
 ** function performs the operation C = A x B efficiently.
 **
 ** INPUT:
 **    C = (*C WRITE) Address of top left element of matrix C.
 **    A = (*A IS READ ONLY) Address of top left element of matrix A.
 **    B = (*B IS READ ONLY) Address of top left element of matrix B.
 **    MatrixSize = Size of matrices (for n*n matrix, MatrixSize = n)
 **    RowWidthA = Number of elements in memory between A[x,y] and A[x,y+1]
 **    RowWidthB = Number of elements in memory between B[x,y] and B[x,y+1]
 **    RowWidthC = Number of elements in memory between C[x,y] and C[x,y+1]
 **
 ** OUTPUT:
 **    C = (*C WRITE) Matrix C contains A x B. (Initial value of *C undefined.)
 **
 *****************************************************************************/

#define strassen(n, A, an, B, bn, C, cn)                                       \
  OptimizedStrassenMultiply(C, A, B, n, cn, bn, an)
void OptimizedStrassenMultiply(REAL *C, REAL *A, REAL *B, unsigned MatrixSize,
                               unsigned RowWidthC, unsigned RowWidthA,
                               unsigned RowWidthB) {

  unsigned QuadrantSize = MatrixSize >> 1; /* MatixSize / 2 */
  unsigned QuadrantSizeInBytes =
      sizeof(REAL) * QuadrantSize * QuadrantSize + 32;

  /************************************************************************
   ** For each matrix A, B, and C, we'll want pointers to each quandrant
   ** in the matrix. These quandrants will be addressed as follows:
   **  --        --
   **  | A11  A12 |
   **  |          |
   **  | A21  A22 |
   **  --        --
   ************************************************************************/

#define T2sMULT C22
#define NumberOfVariables 11

  PTR TempMatrixOffset = 0;
  PTR MatrixOffsetA = 0;
  PTR MatrixOffsetB = 0;

  /* Distance between the end of a matrix row and the start of the next row */
  PTR RowIncrementA = (RowWidthA - QuadrantSize) << 3;
  PTR RowIncrementB = (RowWidthB - QuadrantSize) << 3;
  PTR RowIncrementC = (RowWidthC - QuadrantSize) << 3;

  char *Heap;
  void *StartHeap;

  if (MatrixSize <= SizeAtWhichDivideAndConquerIsMoreEfficient) {
    MultiplyByDivideAndConquer(C, A, B, MatrixSize, RowWidthC, RowWidthA,
                               RowWidthB, 0);

    return;
  }

  /* Initialize quandrant matrices */
#define A11 A
#define B11 B
#define C11 C

  REAL *A12 = A11 + QuadrantSize;
  REAL *B12 = B11 + QuadrantSize;
  REAL *C12 = C11 + QuadrantSize;
  REAL *A21 = A + (RowWidthA * QuadrantSize);
  REAL *B21 = B + (RowWidthB * QuadrantSize);
  REAL *C21 = C + (RowWidthC * QuadrantSize);
  REAL *A22 = A21 + QuadrantSize;
  REAL *B22 = B21 + QuadrantSize;
  REAL *C22 = C21 + QuadrantSize;

  /* Allocate Heap Space Here */
  char *_tmp = (char *)malloc(QuadrantSizeInBytes * NumberOfVariables);
  StartHeap = Heap = _tmp;
  /* ensure that heap is on cache boundary */
  if (((PTR)Heap) & 31)
    Heap = (char *)(((PTR)Heap) + 32 - (((PTR)Heap) & 31));

  /* Distribute the heap space over the variables */
  REAL *S1 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S2 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S3 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S4 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S5 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S6 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S7 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *S8 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *M2 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *M5 = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;
  REAL *T1sMULT = (REAL *)Heap;
  Heap += QuadrantSizeInBytes;

  /***************************************************************************
   ** Step through all columns row by row (vertically)
   ** (jumps in memory by RowWidth => bad locality)
   ** (but we want the best locality on the innermost loop)
   **************************************************************************/
  for (unsigned Row = 0; Row < QuadrantSize; Row++) {

    /*********************************************************************
     ** Step through each row horizontally (addressing elements in
     ** each column) (jumps linearly througn memory => good locality)
     *********************************************************************/
    for (unsigned Column = 0; Column < QuadrantSize; Column++) {

      /***********************************************************
       ** Within this loop, the following holds for MatrixOffset:
       ** MatrixOffset = (Row * RowWidth) + Column
       ** (note: that the unit of the offset is number of reals)
       ***********************************************************/
      /* Element of Global Matrix, such as A, B, C */
#define E(Matrix) (*(REAL *)(((PTR)Matrix) + TempMatrixOffset))
#define EA(Matrix) (*(REAL *)(((PTR)Matrix) + MatrixOffsetA))
#define EB(Matrix) (*(REAL *)(((PTR)Matrix) + MatrixOffsetB))

      /* FIXME - may pay to expand these out - got higher speed-ups below */
      /* S4 = A12 - ( S2 = ( S1 = A21 + A22 ) - A11 ) */
      E(S4) = EA(A12) - (E(S2) = (E(S1) = EA(A21) + EA(A22)) - EA(A11));

      /* S8 = (S6 = B22 - ( S5 = B12 - B11 ) ) - B21 */
      E(S8) = (E(S6) = EB(B22) - (E(S5) = EB(B12) - EB(B11))) - EB(B21);

      /* S3 = A11 - A21 */
      E(S3) = EA(A11) - EA(A21);

      /* S7 = B22 - B12 */
      E(S7) = EB(B22) - EB(B12);

      TempMatrixOffset += sizeof(REAL);
      MatrixOffsetA += sizeof(REAL);
      MatrixOffsetB += sizeof(REAL);
    } /* end row loop*/

    MatrixOffsetA += RowIncrementA;
    MatrixOffsetB += RowIncrementB;
  } /* end column loop */

  cilk_scope {
    /* M2 = A11 x B11 */
    cilk_spawn OptimizedStrassenMultiply(M2, A11, B11, QuadrantSize,
                                         QuadrantSize, RowWidthA, RowWidthB);

    /* M5 = S1 * S5 */
    cilk_spawn OptimizedStrassenMultiply(M5, S1, S5, QuadrantSize, QuadrantSize,
                                         QuadrantSize, QuadrantSize);

    /* Step 1 of T1 = S2 x S6 + M2 */
    cilk_spawn OptimizedStrassenMultiply(T1sMULT, S2, S6, QuadrantSize,
                                         QuadrantSize, QuadrantSize,
                                         QuadrantSize);

    /* Step 1 of T2 = T1 + S3 x S7 */
    cilk_spawn OptimizedStrassenMultiply(C22, S3, S7, QuadrantSize,
                                         RowWidthC /*FIXME*/, QuadrantSize,
                                         QuadrantSize);

    /* Step 1 of C11 = M2 + A12 * B21 */
    cilk_spawn OptimizedStrassenMultiply(C11, A12, B21, QuadrantSize, RowWidthC,
                                         RowWidthA, RowWidthB);

    /* Step 1 of C12 = S4 x B22 + T1 + M5 */
    cilk_spawn OptimizedStrassenMultiply(C12, S4, B22, QuadrantSize, RowWidthC,
                                         QuadrantSize, RowWidthB);

    /* Step 1 of C21 = T2 - A22 * S8 */
    OptimizedStrassenMultiply(C21, A22, S8, QuadrantSize, RowWidthC, RowWidthA,
                              QuadrantSize);

    /**********************************************
     ** Synchronization Point
     **********************************************/
  }

  /*************************************************************************
   ** Step through all columns row by row (vertically)
   ** (jumps in memory by RowWidth => bad locality)
   ** (but we want the best locality on the innermost loop)
   *************************************************************************/
  for (unsigned Row = 0; Row < QuadrantSize; Row++) {

    /*********************************************************************
     ** Step through each row horizontally (addressing elements in
     ** each column) (jumps linearly througn memory => good locality)
     *********************************************************************/
    for (unsigned Column = 0; Column < QuadrantSize; Column += 4) {
      REAL LocalM5_0 = *(M5);
      REAL LocalM5_1 = *(M5 + 1);
      REAL LocalM5_2 = *(M5 + 2);
      REAL LocalM5_3 = *(M5 + 3);
      REAL LocalM2_0 = *(M2);
      REAL LocalM2_1 = *(M2 + 1);
      REAL LocalM2_2 = *(M2 + 2);
      REAL LocalM2_3 = *(M2 + 3);
      REAL T1_0 = *(T1sMULT) + LocalM2_0;
      REAL T1_1 = *(T1sMULT + 1) + LocalM2_1;
      REAL T1_2 = *(T1sMULT + 2) + LocalM2_2;
      REAL T1_3 = *(T1sMULT + 3) + LocalM2_3;
      REAL T2_0 = *(C22) + T1_0;
      REAL T2_1 = *(C22 + 1) + T1_1;
      REAL T2_2 = *(C22 + 2) + T1_2;
      REAL T2_3 = *(C22 + 3) + T1_3;
      (*(C11)) += LocalM2_0;
      (*(C11 + 1)) += LocalM2_1;
      (*(C11 + 2)) += LocalM2_2;
      (*(C11 + 3)) += LocalM2_3;
      (*(C12)) += LocalM5_0 + T1_0;
      (*(C12 + 1)) += LocalM5_1 + T1_1;
      (*(C12 + 2)) += LocalM5_2 + T1_2;
      (*(C12 + 3)) += LocalM5_3 + T1_3;
      (*(C22)) = LocalM5_0 + T2_0;
      (*(C22 + 1)) = LocalM5_1 + T2_1;
      (*(C22 + 2)) = LocalM5_2 + T2_2;
      (*(C22 + 3)) = LocalM5_3 + T2_3;
      (*(C21)) = (-*(C21)) + T2_0;
      (*(C21 + 1)) = (-*(C21 + 1)) + T2_1;
      (*(C21 + 2)) = (-*(C21 + 2)) + T2_2;
      (*(C21 + 3)) = (-*(C21 + 3)) + T2_3;
      M5 += 4;
      M2 += 4;
      T1sMULT += 4;
      C11 += 4;
      C12 += 4;
      C21 += 4;
      C22 += 4;
    }

    C11 = (REAL *)(((PTR)C11) + RowIncrementC);
    C12 = (REAL *)(((PTR)C12) + RowIncrementC);
    C21 = (REAL *)(((PTR)C21) + RowIncrementC);
    C22 = (REAL *)(((PTR)C22) + RowIncrementC);
  }
  free(StartHeap);

  return;
}

/*
 * Set an size n vector V to random values.
 */
void init_vec(int n, REAL *V) {
  int i;

  for (i = 0; i < n; i++) {
    V[i] = ((double)cilk_rand()) / (double)RAND_MAX;
  }
}

/*
 * Compare two matrices.  Return -1 if they differ more EPSILON.
 */
int compare_vec(int n, REAL *V1, REAL *V2) {
  REAL sum = 0.0;

  for (int i = 0; i < n; ++i) {
    REAL c = V1[i] - V2[i];
    if (c < 0.0) {
      c = -c;
    }
    sum += c;
    // ANGE: this is used in compare_matrix
    // c = c / V1[i];
    if (c > EPSILON) {
      return -1;
    }
  }

  printf("Sum of errors: %g\n", sum);
  return 0;
}

/*
 * Allocate a vector of size n
 */
REAL *alloc_vec(int n) { return (REAL *)malloc(n * sizeof(REAL)); }

/*
 * free a vector
 */
void free_vec(REAL *V) { free(V); }

/*
 * Set an n by n matrix A to random values.  The distance between
 * rows is an
 */
void init_matrix(int n, REAL *A, int an) {

  for (int i = 0; i < n; ++i)
    for (int j = 0; j < n; ++j)
      ELEM(A, an, i, j) = ((double)cilk_rand()) / (double)RAND_MAX;
}

/*
 * Compare two matrices.  Print an error message if they differ by
 * more than EPSILON.
 */
int compare_matrix(int n, REAL *A, int an, REAL *B, int bn) {

  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      /* compute the relative error c */
      REAL c = ELEM(A, an, i, j) - ELEM(B, bn, i, j);
      if (c < 0.0)
        c = -c;

      c = c / ELEM(A, an, i, j);
      if (c > EPSILON) {
        return -1;
      }
    }
  }

  return 0;
}

/*
 * Allocate a matrix of side n (therefore n^2 elements)
 */
REAL *alloc_matrix(int n) { return (REAL *)malloc(n * n * sizeof(REAL)); }

/*
 * free a matrix (Never used because Matteo expects
 *                the OS to clean up his garbage. Tsk. Tsk.)
 */
void free_matrix(REAL *A) { free(A); }

/*
 * simple test program
 */
int usage(void) {
  fprintf(stderr,
          "\nUsage: strassen [<cilk-options>] [-n #] [-c] [-rc]\n\n"
          "Multiplies two randomly generated n x n matrices. To check for\n"
          "correctness use -c using iterative matrix multiply or use -rc \n"
          "using randomized algorithm due to Freivalds.\n\n");

  return 1;
}

const char *specifiers[] = {"-n", "-c", "-rc", "-benchmark", "-h", 0};
int opt_types[] = {INTARG, BOOLARG, BOOLARG, BENCHMARK, BOOLARG, 0};

int main(int argc, char *argv[]) {

  int benchmark, help;

  /* standard benchmark options*/
  int n = 512;
  int verify = 0;
  int rand_check = 0;

  get_options(argc, argv, specifiers, opt_types, &n, &verify, &rand_check,
              &benchmark, &help);

  if (help)
    return usage();

  if (benchmark) {
    switch (benchmark) {
    case 1: /* short benchmark options -- a little work*/
      n = 512;
      break;
    case 2: /* standard benchmark options*/
      n = 2048;
      break;
    case 3: /* long benchmark options -- a lot of work*/
      n = 4096;
      break;
    }
  }

  if ((n & (n - 1)) != 0 || (n % 16) != 0) {
    printf("%d: matrix size must be a power of 2"
           " and a multiple of %d\n",
           n, 16);
    return 1;
  }

  REAL *A = alloc_matrix(n);
  REAL *B = alloc_matrix(n);
  REAL *C = alloc_matrix(n);

  init_matrix(n, A, n);
  init_matrix(n, B, n);

  struct timeval t1, t2;
  gettimeofday(&t1, 0);

  strassen(n, A, n, B, n, C, n);

  gettimeofday(&t2, 0);
  unsigned long long runtime_ms = (todval(&t2) - todval(&t1)) / 1000;
  printf("%f\n", runtime_ms / 1000.0);

  if (rand_check) {
    REAL *R, *V1, *V2;
    R = alloc_vec(n);
    V1 = alloc_vec(n);
    V2 = alloc_vec(n);

    mat_vec_mul(n, n, n, B, R, V1, 0);
    mat_vec_mul(n, n, n, A, V1, V2, 0);
    mat_vec_mul(n, n, n, C, R, V1, 0);
    rand_check = compare_vec(n, V1, V2);

    free_vec(R);
    free_vec(V1);
    free_vec(V2);

  } else if (verify) {
    fprintf(stderr, "Checking results ... \n");
    REAL *C2 = alloc_matrix(n);
    matrixmul(n, A, n, B, n, C2, n);
    verify = compare_matrix(n, C, n, C2, n);
    free_matrix(C2);
  }

  if (rand_check || verify) {
    fprintf(stderr, "WRONG RESULT!\n");

  } else {
    fprintf(stderr, "\nCilk Example: strassen\n");
    fprintf(stderr, "Options: n = %d\n\n", n);
  }

  free_matrix(A);
  free_matrix(B);
  free_matrix(C);

  return 0;
}
