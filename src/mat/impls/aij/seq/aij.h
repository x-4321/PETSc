/* $Id: aij.h,v 1.43 2001/03/23 22:04:55 bsmith Exp bsmith $ */

#include "src/mat/matimpl.h"

#if !defined(__AIJ_H)
#define __AIJ_H

/* Info about i-nodes (identical nodes) */
typedef struct {
  PetscTruth use;
  int        node_count;                    /* number of inodes */
  int        *size;                         /* size of each inode */
  int        limit;                         /* inode limit */
  int        max_limit;                     /* maximum supported inode limit */
  PetscTruth checked;                       /* if inodes have been checked for */
} Mat_SeqAIJ_Inode;

/*  
  MATSEQAIJ format - Compressed row storage (also called Yale sparse matrix
  format), compatible with Fortran.  The i[] and j[] arrays start at 1,
  or 0, depending on the value of shift.  For example, in Fortran 
  j[i[k]+p+shift] is the pth column in row k.  Note that the diagonal
  matrix elements are stored with the rest of the nonzeros (not separately).
*/

typedef struct {
  PetscTruth       sorted;           /* if true, rows are sorted by increasing columns */
  PetscTruth       roworiented;      /* if true, row-oriented input, default */
  int              nonew;            /* 1 don't add new nonzeros, -1 generate error on new */
  PetscTruth       singlemalloc;     /* if true a, i, and j have been obtained with
                                          one big malloc */
  PetscTruth       freedata;        /* free the i,j,a data when the matrix is destroyed; true by default */
  int              nz,maxnz;        /* nonzeros, allocated nonzeros */
  int              *diag;            /* pointers to diagonal elements */
  int              *i;               /* pointer to beginning of each row */
  int              *imax;            /* maximum space allocated for each row */
  int              *ilen;            /* actual length of each row */
  int              *j;               /* column values: j + i[k] - 1 is start of row k */
  Scalar           *a;               /* nonzero elements */
  IS               row,col,icol;   /* index sets, used for reorderings */
  Scalar           *solve_work;      /* work space used in MatSolve */
  void             *spptr;           /* pointer for special library like SuperLU */
  int              indexshift;       /* zero or -one for C or Fortran indexing */
  Mat_SeqAIJ_Inode inode;            /* identical node informaton */
  int              reallocs;         /* number of mallocs done during MatSetValues() 
                                        as more values are set than were prealloced */
  int              rmax;             /* max nonzeros in any row */
  PetscTruth       ilu_preserve_row_sums;
  PetscReal        lu_dtcol;
  PetscTruth       lu_damp;
  PetscReal        lu_damping;
  Scalar           *saved_values;    /* location for stashing nonzero values of matrix */
  Scalar           *idiag,*ssor;     /* inverse of diagonal entries; space for eisen */

  PetscTruth       keepzeroedrows;   /* keeps matrix structure same in calls to MatZeroRows()*/
  PetscTruth       ignorezeroentries;
  ISColoring       coloring;         /* set with MatADSetColoring() used by MatADSetValues() */
} Mat_SeqAIJ;

EXTERN int MatILUFactorSymbolic_SeqAIJ(Mat,IS,IS,MatILUInfo*,Mat *);
EXTERN int MatDuplicate_SeqAIJ(Mat,MatDuplicateOption,Mat*);
EXTERN int MatMarkDiagonal_SeqAIJ(Mat);

EXTERN int MatMult_SeqAIJ(Mat A,Vec,Vec);
EXTERN int MatMultAdd_SeqAIJ(Mat A,Vec,Vec,Vec);
EXTERN int MatMultTranspose_SeqAIJ(Mat A,Vec,Vec);
EXTERN int MatMultTransposeAdd_SeqAIJ(Mat A,Vec,Vec,Vec);
EXTERN int MatRelax_SeqAIJ(Mat,Vec,double,MatSORType,double,int,Vec);

EXTERN int MatSetColoring_SeqAIJ(Mat,ISColoring);
EXTERN int MatSetValuesAdic_SeqAIJ(Mat,void*);
EXTERN int MatSetValuesAdifor_SeqAIJ(Mat,int,void*);

#endif
