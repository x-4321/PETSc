/*$Id: aijnode.c,v 1.125 2001/04/10 19:35:19 bsmith Exp bsmith $*/
/*
  This file provides high performance routines for the AIJ (compressed row)
  format by taking advantage of rows with identical nonzero structure (I-nodes).
*/
#include "src/mat/impls/aij/seq/aij.h"                
#include "src/vec/vecimpl.h"

EXTERN int Mat_AIJ_CheckInode(Mat,PetscTruth);
EXTERN int MatSolve_SeqAIJ_Inode(Mat,Vec,Vec);
EXTERN int MatLUFactorNumeric_SeqAIJ_Inode(Mat,Mat *);

EXTERN int MatMult_SeqAIJ(Mat,Vec,Vec);
EXTERN int MatMultAdd_SeqAIJ(Mat,Vec,Vec,Vec);
EXTERN int MatSolve_SeqAIJ(Mat,Vec,Vec);
EXTERN int MatLUFactorNumeric_SeqAIJ(Mat,Mat*);
EXTERN int MatGetRowIJ_SeqAIJ(Mat,int,PetscTruth,int*,int**,int**,PetscTruth*);
EXTERN int MatRestoreRowIJ_SeqAIJ(Mat,int,PetscTruth,int*,int**,int**,PetscTruth*);
EXTERN int MatGetColumnIJ_SeqAIJ(Mat,int,PetscTruth,int*,int**,int**,PetscTruth*);
EXTERN int MatRestoreColumnIJ_SeqAIJ(Mat,int,PetscTruth,int*,int**,int**,PetscTruth*);

#undef __FUNCT__  
#define __FUNCT__ "Mat_AIJ_CreateColInode"
static int Mat_AIJ_CreateColInode(Mat A,int* size,int ** ns)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        ierr,i,count,m,n,min_mn,*ns_row,*ns_col;

  PetscFunctionBegin;  
  n      = A->n;
  m      = A->m;
  ns_row = a->inode.size;
  
  min_mn = (m < n) ? m : n;
  if (!ns) {
    for (count=0,i=0; count<min_mn; count+=ns_row[i],i++);
    for(; count+1 < n; count++,i++);
    if (count < n)  {
      i++;
    }
    *size = i;
    PetscFunctionReturn(0);
  }
  ierr = PetscMalloc((n+1)*sizeof(int),&ns_col);CHKERRQ(ierr);
  
  /* Use the same row structure wherever feasible. */
  for (count=0,i=0; count<min_mn; count+=ns_row[i],i++) {
    ns_col[i] = ns_row[i];
  }

  /* if m < n; pad up the remainder with inode_limit */
  for(; count+1 < n; count++,i++) {
    ns_col[i] = 1;
  }
  /* The last node is the odd ball. padd it up with the remaining rows; */
  if (count < n)  {
    ns_col[i] = n - count;
    i++;
  } else if (count > n) {
    /* Adjust for the over estimation */
    ns_col[i-1] += n - count;
  }
  *size = i;
  *ns   = ns_col;
  PetscFunctionReturn(0);
}


/*
      This builds symmetric version of nonzero structure,
*/
#undef __FUNCT__  
#define __FUNCT__ "MatGetRowIJ_SeqAIJ_Inode_Symmetric"
static int MatGetRowIJ_SeqAIJ_Inode_Symmetric(Mat A,int **iia,int **jja,int ishift,int oshift)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        *work,*ia,*ja,*j,nz,nslim_row,nslim_col,m,row,col,*jmax,n,ierr;
  int        *tns,*tvc,*ns_row = a->inode.size,*ns_col,nsz,i1,i2,*ai= a->i,*aj = a->j;

  PetscFunctionBegin;  
  nslim_row = a->inode.node_count;
  m         = A->m;
  n         = A->n;
  if (m != n) SETERRQ(1,"MatGetRowIJ_SeqAIJ_Inode_Symmetric: Matrix shoul be square");
  
  /* Use the row_inode as column_inode */
  nslim_col = nslim_row;
  ns_col    = ns_row;

  /* allocate space for reformated inode structure */
  ierr = PetscMalloc((nslim_col+1)*sizeof(int),&tns);CHKERRQ(ierr);
  ierr = PetscMalloc((n+1)*sizeof(int),&tvc);CHKERRQ(ierr);
  for (i1=0,tns[0]=0; i1<nslim_col; ++i1) tns[i1+1] = tns[i1]+ ns_row[i1];

  for (i1=0,col=0; i1<nslim_col; ++i1){
    nsz = ns_col[i1];
    for (i2=0; i2<nsz; ++i2,++col)
      tvc[col] = i1;
  }
  /* allocate space for row pointers */
  ierr = PetscMalloc((nslim_row+1)*sizeof(int),&ia);CHKERRQ(ierr);
  *iia = ia;
  ierr = PetscMemzero(ia,(nslim_row+1)*sizeof(int));CHKERRQ(ierr);
  ierr = PetscMalloc((nslim_row+1)*sizeof(int),&work);CHKERRQ(ierr);

  /* determine the number of columns in each row */
  ia[0] = oshift;
  for (i1=0,row=0 ; i1<nslim_row; row+=ns_row[i1],i1++) {

    j    = aj + ai[row] + ishift;
    jmax = aj + ai[row+1] + ishift;
    i2   = 0;
    col  = *j++ + ishift;
    i2   = tvc[col];
    while (i2<i1 && j<jmax) { /* 1.[-xx-d-xx--] 2.[-xx-------],off-diagonal elemets */
      ia[i1+1]++;
      ia[i2+1]++;
      i2++;                     /* Start col of next node */
      while(((col=*j+ishift)<tns[i2]) && (j<jmax)) ++j;
      i2 = tvc[col];
    }
    if(i2 == i1) ia[i2+1]++;    /* now the diagonal element */
  }

  /* shift ia[i] to point to next row */
  for (i1=1; i1<nslim_row+1; i1++) {
    row        = ia[i1-1];
    ia[i1]    += row;
    work[i1-1] = row - oshift;
  }

  /* allocate space for column pointers */
  nz   = ia[nslim_row] + (!ishift);
  ierr = PetscMalloc(nz*sizeof(int),&ja);CHKERRQ(ierr);
  *jja = ja;

 /* loop over lower triangular part putting into ja */ 
  for (i1=0,row=0; i1<nslim_row; row += ns_row[i1],i1++) {
    j    = aj + ai[row] + ishift;
    jmax = aj + ai[row+1] + ishift;
    i2   = 0;                     /* Col inode index */
    col  = *j++ + ishift;
    i2   = tvc[col];
    while (i2<i1 && j<jmax) {
      ja[work[i2]++] = i1 + oshift;
      ja[work[i1]++] = i2 + oshift;
      ++i2;
      while(((col=*j+ishift)< tns[i2])&&(j<jmax)) ++j; /* Skip rest col indices in this node */
      i2 = tvc[col];
    }
    if (i2 == i1) ja[work[i1]++] = i2 + oshift;

  }
  ierr = PetscFree(work);CHKERRQ(ierr);
  ierr = PetscFree(tns);CHKERRQ(ierr);
  ierr = PetscFree(tvc);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*
      This builds nonsymmetric version of nonzero structure,
*/
#undef __FUNCT__  
#define __FUNCT__ "MatGetRowIJ_SeqAIJ_Inode_Nonsymmetric"
static int MatGetRowIJ_SeqAIJ_Inode_Nonsymmetric(Mat A,int **iia,int **jja,int ishift,int oshift)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        *work,*ia,*ja,*j,nz,nslim_row,n,row,col,ierr,*ns_col,nslim_col;
  int        *tns,*tvc,*ns_row = a->inode.size,nsz,i1,i2,*ai= a->i,*aj = a->j;

  PetscFunctionBegin;  
  nslim_row = a->inode.node_count;
  n         = A->n;

  /* Create The column_inode for this matrix */
  ierr = Mat_AIJ_CreateColInode(A,&nslim_col,&ns_col);CHKERRQ(ierr);
  
  /* allocate space for reformated column_inode structure */
  ierr = PetscMalloc((nslim_col +1)*sizeof(int),&tns);CHKERRQ(ierr);
  ierr = PetscMalloc((n +1)*sizeof(int),&tvc);CHKERRQ(ierr);
  for (i1=0,tns[0]=0; i1<nslim_col; ++i1) tns[i1+1] = tns[i1] + ns_col[i1];

  for (i1=0,col=0; i1<nslim_col; ++i1){
    nsz = ns_col[i1];
    for (i2=0; i2<nsz; ++i2,++col)
      tvc[col] = i1;
  }
  /* allocate space for row pointers */
  ierr = PetscMalloc((nslim_row+1)*sizeof(int),&ia);CHKERRQ(ierr);
  *iia = ia;
  ierr = PetscMemzero(ia,(nslim_row+1)*sizeof(int));CHKERRQ(ierr);
  ierr = PetscMalloc((nslim_row+1)*sizeof(int),&work);CHKERRQ(ierr);

  /* determine the number of columns in each row */
  ia[0] = oshift;
  for (i1=0,row=0; i1<nslim_row; row+=ns_row[i1],i1++) {
    j   = aj + ai[row] + ishift;
    col = *j++ + ishift;
    i2  = tvc[col];
    nz  = ai[row+1] - ai[row]; 
    while (nz-- > 0) {           /* off-diagonal elemets */
      ia[i1+1]++;
      i2++;                     /* Start col of next node */
      while (((col = *j++ + ishift) < tns[i2]) && nz > 0) {nz--;}
      i2 = tvc[col];
    }
  }

  /* shift ia[i] to point to next row */
  for (i1=1; i1<nslim_row+1; i1++) {
    row        = ia[i1-1];
    ia[i1]    += row;
    work[i1-1] = row - oshift;
  }

  /* allocate space for column pointers */
  nz   = ia[nslim_row] + (!ishift);
  ierr = PetscMalloc(nz*sizeof(int),&ja);CHKERRQ(ierr);
  *jja = ja;

 /* loop over matrix putting into ja */ 
  for (i1=0,row=0; i1<nslim_row; row+=ns_row[i1],i1++) {
    j   = aj + ai[row] + ishift;
    i2  = 0;                     /* Col inode index */
    col = *j++ + ishift;
    i2  = tvc[col];
    nz  = ai[row+1] - ai[row]; 
    while (nz-- > 0) {
      ja[work[i1]++] = i2 + oshift;
      ++i2;
      while(((col = *j++ + ishift) < tns[i2]) && nz > 0) {nz--;}
      i2 = tvc[col];
    }
  }
  ierr = PetscFree(ns_col);CHKERRQ(ierr);
  ierr = PetscFree(work);CHKERRQ(ierr);
  ierr = PetscFree(tns);CHKERRQ(ierr);
  ierr = PetscFree(tvc);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetRowIJ_SeqAIJ_Inode"
static int MatGetRowIJ_SeqAIJ_Inode(Mat A,int oshift,PetscTruth symmetric,int *n,int **ia,int **ja,PetscTruth *done)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        ierr,ishift;

  PetscFunctionBegin;  
  *n     = a->inode.node_count;
  if (!ia) PetscFunctionReturn(0);

  ishift = a->indexshift;
  if (symmetric) {
    ierr = MatGetRowIJ_SeqAIJ_Inode_Symmetric(A,ia,ja,ishift,oshift);CHKERRQ(ierr);
  } else {
    ierr = MatGetRowIJ_SeqAIJ_Inode_Nonsymmetric(A,ia,ja,ishift,oshift);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreRowIJ_SeqAIJ_Inode"
static int MatRestoreRowIJ_SeqAIJ_Inode(Mat A,int oshift,PetscTruth symmetric,int *n,int **ia,int **ja,PetscTruth *done)
{
  int ierr;

  PetscFunctionBegin;  
  if (!ia) PetscFunctionReturn(0);
  ierr = PetscFree(*ia);CHKERRQ(ierr);
  ierr = PetscFree(*ja);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */

#undef __FUNCT__  
#define __FUNCT__ "MatGetColumnIJ_SeqAIJ_Inode_Nonsymmetric" 
static int MatGetColumnIJ_SeqAIJ_Inode_Nonsymmetric(Mat A,int **iia,int **jja,int ishift,int oshift)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int *work,*ia,*ja,*j,nz,nslim_row, n,row,col,ierr,*ns_col,nslim_col;
  int *tns,*tvc,*ns_row = a->inode.size,nsz,i1,i2,*ai= a->i,*aj = a->j;

  PetscFunctionBegin;  
  nslim_row = a->inode.node_count;
  n         = A->n;

  /* Create The column_inode for this matrix */
  ierr = Mat_AIJ_CreateColInode(A,&nslim_col,&ns_col);CHKERRQ(ierr);
  
  /* allocate space for reformated column_inode structure */
  ierr = PetscMalloc((nslim_col + 1)*sizeof(int),&tns);CHKERRQ(ierr);
  ierr = PetscMalloc((n + 1)*sizeof(int),&tvc);CHKERRQ(ierr);
  for (i1=0,tns[0]=0; i1<nslim_col; ++i1) tns[i1+1] = tns[i1] + ns_col[i1];

  for (i1=0,col=0; i1<nslim_col; ++i1){
    nsz = ns_col[i1];
    for (i2=0; i2<nsz; ++i2,++col)
      tvc[col] = i1;
  }
  /* allocate space for column pointers */
  ierr = PetscMalloc((nslim_col+1)*sizeof(int),&ia);CHKERRQ(ierr);
  *iia = ia;
  ierr = PetscMemzero(ia,(nslim_col+1)*sizeof(int));CHKERRQ(ierr);
  ierr = PetscMalloc((nslim_col+1)*sizeof(int),&work);CHKERRQ(ierr);

  /* determine the number of columns in each row */
  ia[0] = oshift;
  for (i1=0,row=0; i1<nslim_row; row+=ns_row[i1],i1++) {
    j   = aj + ai[row] + ishift;
    col = *j++ + ishift;
    i2  = tvc[col];
    nz  = ai[row+1] - ai[row]; 
    while (nz-- > 0) {           /* off-diagonal elemets */
      /* ia[i1+1]++; */
      ia[i2+1]++;
      i2++;      
      while (((col = *j++ + ishift) < tns[i2]) && nz > 0) {nz--;}
      i2 = tvc[col];
    }
  }

  /* shift ia[i] to point to next col */
  for (i1=1; i1<nslim_col+1; i1++) {
    col        = ia[i1-1];
    ia[i1]    += col;
    work[i1-1] = col - oshift;
  }

  /* allocate space for column pointers */
  nz   = ia[nslim_col] + (!ishift);
  ierr = PetscMalloc(nz*sizeof(int),&ja);CHKERRQ(ierr);
  *jja = ja;

 /* loop over matrix putting into ja */ 
  for (i1=0,row=0; i1<nslim_row; row+=ns_row[i1],i1++) {
    j   = aj + ai[row] + ishift;
    i2  = 0;                     /* Col inode index */
    col = *j++ + ishift;
    i2  = tvc[col];
    nz  = ai[row+1] - ai[row]; 
    while (nz-- > 0) {
      /* ja[work[i1]++] = i2 + oshift; */
      ja[work[i2]++] = i1 + oshift;
      i2++;
      while(((col = *j++ + ishift) < tns[i2]) && nz > 0) {nz--;}
      i2 = tvc[col];
    }
  }
  ierr = PetscFree(ns_col);CHKERRQ(ierr);
  ierr = PetscFree(work);CHKERRQ(ierr);
  ierr = PetscFree(tns);CHKERRQ(ierr);
  ierr = PetscFree(tvc);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatGetColumnIJ_SeqAIJ_Inode"
static int MatGetColumnIJ_SeqAIJ_Inode(Mat A,int oshift,PetscTruth symmetric,int *n,int **ia,int **ja,PetscTruth *done)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        ierr,ishift;

  PetscFunctionBegin;  
  ierr = Mat_AIJ_CreateColInode(A,n,PETSC_NULL);CHKERRQ(ierr);
  if (!ia) PetscFunctionReturn(0);

  ishift = a->indexshift;
  if (symmetric) {
    /* Since the indices are symmetric it does'nt matter */
    ierr = MatGetRowIJ_SeqAIJ_Inode_Symmetric(A,ia,ja,ishift,oshift);CHKERRQ(ierr);
  } else {
    ierr = MatGetColumnIJ_SeqAIJ_Inode_Nonsymmetric(A,ia,ja,ishift,oshift);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatRestoreColumnIJ_SeqAIJ_Inode"
static int MatRestoreColumnIJ_SeqAIJ_Inode(Mat A,int oshift,PetscTruth symmetric,int *n,int **ia,int **ja,PetscTruth *done)
{
  int ierr;

  PetscFunctionBegin;  
  if (!ia) PetscFunctionReturn(0);
  ierr = PetscFree(*ia);CHKERRQ(ierr);
  ierr = PetscFree(*ja);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */

#undef __FUNCT__  
#define __FUNCT__ "MatMult_SeqAIJ_Inode"
static int MatMult_SeqAIJ_Inode(Mat A,Vec xx,Vec yy)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data; 
  Scalar     sum1,sum2,sum3,sum4,sum5,tmp0,tmp1;
  Scalar     *v1,*v2,*v3,*v4,*v5,*x,*y;
  int        ierr,*idx,i1,i2,n,i,row,node_max,*ns,*ii,nsz,sz;
  int        shift = a->indexshift;
  
#if defined(PETSC_HAVE_PRAGMA_DISJOINT)
#pragma disjoint(*x,*y,*v1,*v2,*v3,*v4,*v5)
#endif

  PetscFunctionBegin;  
  if (!a->inode.size) SETERRQ(PETSC_ERR_COR,"Missing Inode Structure");
  node_max = a->inode.node_count;                
  ns       = a->inode.size;     /* Node Size array */
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  x    = x + shift;             /* shift for Fortran start by 1 indexing */
  idx  = a->j;
  v1   = a->a;
  ii   = a->i;

  for (i = 0,row = 0; i< node_max; ++i){
    nsz  = ns[i]; 
    n    = ii[1] - ii[0];
    ii  += nsz;
    sz   = n;                   /*No of non zeros in this row */
                                /* Switch on the size of Node */
    switch (nsz){               /* Each loop in 'case' is unrolled */
    case 1 :
      sum1  = 0;
      
      for(n = 0; n< sz-1; n+=2) {
        i1   = idx[0];          /* The instructions are ordered to */
        i2   = idx[1];          /* make the compiler's job easy */
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
       }
     
      if (n == sz-1){          /* Take care of the last nonzero  */
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
      }
      y[row++]=sum1;
      break;
    case 2:
      sum1  = 0;
      sum2  = 0;
      v2    = v1 + n;
      
      for (n = 0; n< sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2];
        sum1 += v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
      }
      if (n == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      v1      =v2;              /* Since the next block to be processed starts there*/
      idx    +=sz;
      break;
    case 3:
      sum1  = 0;
      sum2  = 0;
      sum3  = 0;
      v2    = v1 + n;
      v3    = v2 + n;
      
      for (n = 0; n< sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
        sum3 += v3[0] * tmp0 + v3[1] * tmp1; v3 += 2;
      }
      if (n == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
        sum3 += *v3++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      y[row++]=sum3;
      v1       =v3;             /* Since the next block to be processed starts there*/
      idx     +=2*sz;
      break;
    case 4:
      sum1  = 0;
      sum2  = 0;
      sum3  = 0;
      sum4  = 0;
      v2    = v1 + n;
      v3    = v2 + n;
      v4    = v3 + n;
      
      for (n = 0; n< sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] *tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] *tmp1; v2 += 2;
        sum3 += v3[0] * tmp0 + v3[1] *tmp1; v3 += 2;
        sum4 += v4[0] * tmp0 + v4[1] *tmp1; v4 += 2;
      }
      if (n == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
        sum3 += *v3++ * tmp0;
        sum4 += *v4++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      y[row++]=sum3;
      y[row++]=sum4;
      v1      =v4;              /* Since the next block to be processed starts there*/
      idx    +=3*sz;
      break;
    case 5:
      sum1  = 0;
      sum2  = 0;
      sum3  = 0;
      sum4  = 0;
      sum5  = 0;
      v2    = v1 + n;
      v3    = v2 + n;
      v4    = v3 + n;
      v5    = v4 + n;
      
      for (n = 0; n<sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] *tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] *tmp1; v2 += 2;
        sum3 += v3[0] * tmp0 + v3[1] *tmp1; v3 += 2;
        sum4 += v4[0] * tmp0 + v4[1] *tmp1; v4 += 2;
        sum5 += v5[0] * tmp0 + v5[1] *tmp1; v5 += 2;
      }
      if (n == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
        sum3 += *v3++ * tmp0;
        sum4 += *v4++ * tmp0;
        sum5 += *v5++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      y[row++]=sum3;
      y[row++]=sum4;
      y[row++]=sum5;
      v1      =v5;       /* Since the next block to be processed starts there */
      idx    +=4*sz;
      break;
    default :
      SETERRQ(PETSC_ERR_COR,"Node size not yet supported");
    }
  }
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  PetscLogFlops(2*a->nz - A->m);
  PetscFunctionReturn(0);
}
/* ----------------------------------------------------------- */
/* Almost same code as the MatMult_SeqAij_Inode() */
#undef __FUNCT__  
#define __FUNCT__ "MatMultAdd_SeqAIJ_Inode"
static int MatMultAdd_SeqAIJ_Inode(Mat A,Vec xx,Vec zz,Vec yy)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data; 
  Scalar     sum1,sum2,sum3,sum4,sum5,tmp0,tmp1;
  Scalar     *v1,*v2,*v3,*v4,*v5,*x,*y,*z;
  int        ierr,*idx,i1,i2,n,i,row,node_max,*ns,*ii,nsz,sz;
  int        shift = a->indexshift;
  
  PetscFunctionBegin;  
  if (!a->inode.size) SETERRQ(PETSC_ERR_COR,"Missing Inode Structure");
  node_max = a->inode.node_count;                
  ns       = a->inode.size;     /* Node Size array */
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  ierr = VecGetArray(yy,&y);CHKERRQ(ierr);
  if (zz != yy) {
    ierr = VecGetArray(zz,&z);CHKERRQ(ierr);
  } else {
    z = y;
  }

  x    = x + shift;             /* shift for Fortran start by 1 indexing */
  idx  = a->j;
  v1   = a->a;
  ii   = a->i;

  for (i = 0,row = 0; i< node_max; ++i){
    nsz  = ns[i]; 
    n    = ii[1] - ii[0];
    ii  += nsz;
    sz   = n;                   /*No of non zeros in this row */
                                /* Switch on the size of Node */
    switch (nsz){               /* Each loop in 'case' is unrolled */
    case 1 :
      sum1  = *z++;
      
      for(n = 0; n< sz-1; n+=2) {
        i1   = idx[0];          /* The instructions are ordered to */
        i2   = idx[1];          /* make the compiler's job easy */
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
       }
     
      if(n   == sz-1){          /* Take care of the last nonzero  */
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
      }
      y[row++]=sum1;
      break;
    case 2:
      sum1  = *z++;
      sum2  = *z++;
      v2    = v1 + n;
      
      for(n = 0; n< sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2];
        sum1 += v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
      }
      if(n   == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      v1      =v2;              /* Since the next block to be processed starts there*/
      idx    +=sz;
      break;
    case 3:
      sum1  = *z++;
      sum2  = *z++;
      sum3  = *z++;
      v2    = v1 + n;
      v3    = v2 + n;
      
      for (n = 0; n< sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
        sum3 += v3[0] * tmp0 + v3[1] * tmp1; v3 += 2;
      }
      if (n == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
        sum3 += *v3++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      y[row++]=sum3;
      v1       =v3;             /* Since the next block to be processed starts there*/
      idx     +=2*sz;
      break;
    case 4:
      sum1  = *z++;
      sum2  = *z++;
      sum3  = *z++;
      sum4  = *z++;
      v2    = v1 + n;
      v3    = v2 + n;
      v4    = v3 + n;
      
      for (n = 0; n< sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] *tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] *tmp1; v2 += 2;
        sum3 += v3[0] * tmp0 + v3[1] *tmp1; v3 += 2;
        sum4 += v4[0] * tmp0 + v4[1] *tmp1; v4 += 2;
      }
      if (n == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
        sum3 += *v3++ * tmp0;
        sum4 += *v4++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      y[row++]=sum3;
      y[row++]=sum4;
      v1      =v4;              /* Since the next block to be processed starts there*/
      idx    +=3*sz;
      break;
    case 5:
      sum1  = *z++;
      sum2  = *z++;
      sum3  = *z++;
      sum4  = *z++;
      sum5  = *z++;
      v2    = v1 + n;
      v3    = v2 + n;
      v4    = v3 + n;
      v5    = v4 + n;
      
      for (n = 0; n<sz-1; n+=2) {
        i1   = idx[0];
        i2   = idx[1];
        idx += 2;
        tmp0 = x[i1];
        tmp1 = x[i2]; 
        sum1 += v1[0] * tmp0 + v1[1] *tmp1; v1 += 2;
        sum2 += v2[0] * tmp0 + v2[1] *tmp1; v2 += 2;
        sum3 += v3[0] * tmp0 + v3[1] *tmp1; v3 += 2;
        sum4 += v4[0] * tmp0 + v4[1] *tmp1; v4 += 2;
        sum5 += v5[0] * tmp0 + v5[1] *tmp1; v5 += 2;
      }
      if(n   == sz-1){
        tmp0  = x[*idx++];
        sum1 += *v1++ * tmp0;
        sum2 += *v2++ * tmp0;
        sum3 += *v3++ * tmp0;
        sum4 += *v4++ * tmp0;
        sum5 += *v5++ * tmp0;
      }
      y[row++]=sum1;
      y[row++]=sum2;
      y[row++]=sum3;
      y[row++]=sum4;
      y[row++]=sum5;
      v1      =v5;       /* Since the next block to be processed starts there */
      idx    +=4*sz;
      break;
    default :
      SETERRQ(PETSC_ERR_COR,"Node size not yet supported");
    }
  }
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  ierr = VecRestoreArray(yy,&y);CHKERRQ(ierr);
  if (zz != yy) {
    ierr = VecRestoreArray(zz,&z);CHKERRQ(ierr);
  }
  PetscLogFlops(2*a->nz);
  PetscFunctionReturn(0);
}
/* ----------------------------------------------------------- */
EXTERN int MatColoringPatch_SeqAIJ_Inode(Mat,int,int,int *,ISColoring *);

/*
    samestructure indicates that the matrix has not changed its nonzero structure so we 
    do not need to recompute the inodes 
*/
#undef __FUNCT__  
#define __FUNCT__ "Mat_AIJ_CheckInode"
int Mat_AIJ_CheckInode(Mat A,PetscTruth samestructure)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        ierr,i,j,m,nzx,nzy,*idx,*idy,*ns,*ii,node_count,blk_size;
  PetscTruth flag,flg;

  PetscFunctionBegin;  
  if (samestructure && a->inode.checked) PetscFunctionReturn(0);

  a->inode.checked = PETSC_TRUE;

  /* Notes: We set a->inode.limit=5 in MatCreateSeqAIJ(). */
  if (!a->inode.use) {PetscLogInfo(A,"Mat_AIJ_CheckInode: Not using Inode routines due to MatSetOption(MAT_DO_NOT_USE_INODES\n"); PetscFunctionReturn(0);}
  ierr = PetscOptionsHasName(A->prefix,"-mat_aij_no_inode",&flg);CHKERRQ(ierr);
  if (flg) {PetscLogInfo(A,"Mat_AIJ_CheckInode: Not using Inode routines due to -mat_aij_no_inode\n");PetscFunctionReturn(0);}
  ierr = PetscOptionsHasName(A->prefix,"-mat_no_unroll",&flg);CHKERRQ(ierr);
  if (flg) {PetscLogInfo(A,"Mat_AIJ_CheckInode: Not using Inode routines due to -mat_no_unroll\n");PetscFunctionReturn(0);}
  ierr = PetscOptionsGetInt(A->prefix,"-mat_aij_inode_limit",&a->inode.limit,PETSC_NULL);CHKERRQ(ierr);
  if (a->inode.limit > a->inode.max_limit) a->inode.limit = a->inode.max_limit;
  m = A->m;    
  if (a->inode.size) {ns = a->inode.size;}
  else {ierr = PetscMalloc((m+1)*sizeof(int),&ns);CHKERRQ(ierr);}

  i          = 0;
  node_count = 0; 
  idx        = a->j;
  ii         = a->i;
  while (i < m){                /* For each row */
    nzx = ii[i+1] - ii[i];       /* Number of nonzeros */
    /* Limits the number of elements in a node to 'a->inode.limit' */
    for (j=i+1,idy=idx,blk_size=1; j<m && blk_size <a->inode.limit; ++j,++blk_size) {
      nzy     = ii[j+1] - ii[j]; /* Same number of nonzeros */
      if (nzy != nzx) break;
      idy  += nzx;             /* Same nonzero pattern */
      ierr = PetscMemcmp(idx,idy,nzx*sizeof(int),&flag);CHKERRQ(ierr);
      if (flag == PETSC_FALSE) break;
    }
    ns[node_count++] = blk_size;
    idx += blk_size*nzx;
    i    = j;
  }
  /* If not enough inodes found,, do not use inode version of the routines */
  if (!a->inode.size && m && node_count > 0.9*m) {
    ierr = PetscFree(ns);CHKERRQ(ierr);
    A->ops->mult            = MatMult_SeqAIJ;
    A->ops->multadd         = MatMultAdd_SeqAIJ;
    A->ops->solve           = MatSolve_SeqAIJ;
    A->ops->lufactornumeric = MatLUFactorNumeric_SeqAIJ;
    A->ops->getrowij        = MatGetRowIJ_SeqAIJ;
    A->ops->restorerowij    = MatRestoreRowIJ_SeqAIJ;
    A->ops->getcolumnij     = MatGetColumnIJ_SeqAIJ;
    A->ops->restorecolumnij = MatRestoreColumnIJ_SeqAIJ;
    A->ops->coloringpatch   = 0;
    a->inode.node_count     = 0;
    a->inode.size           = 0;
    PetscLogInfo(A,"Mat_AIJ_CheckInode: Found %d nodes out of %d rows. Not using Inode routines\n",node_count,m);
  } else {
    A->ops->mult            = MatMult_SeqAIJ_Inode;
    A->ops->multadd         = MatMultAdd_SeqAIJ_Inode;
    A->ops->solve           = MatSolve_SeqAIJ_Inode;
    A->ops->lufactornumeric = MatLUFactorNumeric_SeqAIJ_Inode;
    A->ops->getrowij        = MatGetRowIJ_SeqAIJ_Inode;
    A->ops->restorerowij    = MatRestoreRowIJ_SeqAIJ_Inode;
    A->ops->getcolumnij     = MatGetColumnIJ_SeqAIJ_Inode;
    A->ops->restorecolumnij = MatRestoreColumnIJ_SeqAIJ_Inode;
    A->ops->coloringpatch   = MatColoringPatch_SeqAIJ_Inode;
    a->inode.node_count     = node_count;
    a->inode.size           = ns;
    PetscLogInfo(A,"Mat_AIJ_CheckInode: Found %d nodes of %d. Limit used: %d. Using Inode routines\n",node_count,m,a->inode.limit);
  }
  PetscFunctionReturn(0);
}

/* ----------------------------------------------------------- */
#undef __FUNCT__  
#define __FUNCT__ "MatSolve_SeqAIJ_Inode"
int MatSolve_SeqAIJ_Inode(Mat A,Vec bb,Vec xx)
{
  Mat_SeqAIJ  *a = (Mat_SeqAIJ*)A->data;
  IS          iscol = a->col,isrow = a->row;
  int         *r,*c,ierr,i,j,n = A->m,*ai = a->i,nz,shift = a->indexshift,*a_j = a->j;
  int         node_max,*ns,row,nsz,aii,*vi,*ad,*aj,i0,i1,*rout,*cout;
  Scalar      *x,*b,*a_a = a->a,*tmp,*tmps,*aa,tmp0,tmp1;
  Scalar      sum1,sum2,sum3,sum4,sum5,*v1,*v2,*v3,*v4,*v5;

  PetscFunctionBegin;  
  if (A->factor!=FACTOR_LU) SETERRQ(PETSC_ERR_ARG_WRONGSTATE,"Not for unfactored matrix");
  if (!a->inode.size) SETERRQ(PETSC_ERR_COR,"Missing Inode Structure");
  node_max = a->inode.node_count;   
  ns       = a->inode.size;     /* Node Size array */

  ierr = VecGetArray(bb,&b);CHKERRQ(ierr);
  ierr = VecGetArray(xx,&x);CHKERRQ(ierr);
  tmp  = a->solve_work;
  
  ierr = ISGetIndices(isrow,&rout);CHKERRQ(ierr); r = rout;
  ierr = ISGetIndices(iscol,&cout);CHKERRQ(ierr); c = cout + (n-1);
  
  /* forward solve the lower triangular */
  tmps = tmp + shift;
  aa   = a_a +shift;
  aj   = a_j + shift;
  ad   = a->diag;

  for (i = 0,row = 0; i< node_max; ++i){
    nsz = ns[i];
    aii = ai[row];
    v1  = aa + aii;
    vi  = aj + aii;
    nz  = ad[row]- aii;
    
    switch (nsz){               /* Each loop in 'case' is unrolled */
    case 1 :
      sum1 = b[*r++];
      /*      while (nz--) sum1 -= *v1++ *tmps[*vi++];*/
      for(j=0; j<nz-1; j+=2){
        i0   = vi[0];
        i1   = vi[1];
        vi  +=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
      }
      if(j == nz-1){
        tmp0 = tmps[*vi++];
        sum1 -= *v1++ *tmp0;
      }
      tmp[row ++]=sum1;
      break;
    case 2:
      sum1 = b[*r++];
      sum2 = b[*r++];
      v2   = aa + ai[row+1];

      for(j=0; j<nz-1; j+=2){
        i0   = vi[0];
        i1   = vi[1];
        vi  +=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 -= v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
      } 
      if(j == nz-1){
        tmp0 = tmps[*vi++];
        sum1 -= *v1++ *tmp0;
        sum2 -= *v2++ *tmp0;
      }
      sum2 -= *v2++ * sum1;
      tmp[row ++]=sum1;
      tmp[row ++]=sum2;
      break;
    case 3:
      sum1 = b[*r++];
      sum2 = b[*r++];
      sum3 = b[*r++];
      v2   = aa + ai[row+1];
      v3   = aa + ai[row+2];
      
      for (j=0; j<nz-1; j+=2){
        i0   = vi[0];
        i1   = vi[1];
        vi  +=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];  
        sum1 -= v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 -= v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
        sum3 -= v3[0] * tmp0 + v3[1] * tmp1; v3 += 2;
      }
      if (j == nz-1){
        tmp0 = tmps[*vi++];
        sum1 -= *v1++ *tmp0;
        sum2 -= *v2++ *tmp0;
        sum3 -= *v3++ *tmp0;
      }
      sum2 -= *v2++ * sum1;
      sum3 -= *v3++ * sum1;
      sum3 -= *v3++ * sum2;
      tmp[row ++]=sum1;
      tmp[row ++]=sum2;
      tmp[row ++]=sum3;
      break;
      
    case 4:
      sum1 = b[*r++];
      sum2 = b[*r++];
      sum3 = b[*r++];
      sum4 = b[*r++];
      v2   = aa + ai[row+1];
      v3   = aa + ai[row+2];
      v4   = aa + ai[row+3];
      
      for (j=0; j<nz-1; j+=2){
        i0   = vi[0];
        i1   = vi[1];
        vi  +=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];   
        sum1 -= v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 -= v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
        sum3 -= v3[0] * tmp0 + v3[1] * tmp1; v3 += 2;
        sum4 -= v4[0] * tmp0 + v4[1] * tmp1; v4 += 2;
      }
      if (j == nz-1){
        tmp0 = tmps[*vi++];
        sum1 -= *v1++ *tmp0;
        sum2 -= *v2++ *tmp0;
        sum3 -= *v3++ *tmp0;
        sum4 -= *v4++ *tmp0;
      }
      sum2 -= *v2++ * sum1;
      sum3 -= *v3++ * sum1;
      sum4 -= *v4++ * sum1;
      sum3 -= *v3++ * sum2;
      sum4 -= *v4++ * sum2;
      sum4 -= *v4++ * sum3;
      
      tmp[row ++]=sum1;
      tmp[row ++]=sum2;
      tmp[row ++]=sum3;
      tmp[row ++]=sum4;
      break;
    case 5:
      sum1 = b[*r++];
      sum2 = b[*r++];
      sum3 = b[*r++];
      sum4 = b[*r++];
      sum5 = b[*r++];
      v2   = aa + ai[row+1];
      v3   = aa + ai[row+2];
      v4   = aa + ai[row+3];
      v5   = aa + ai[row+4];
      
      for (j=0; j<nz-1; j+=2){
        i0   = vi[0];
        i1   = vi[1];
        vi  +=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];   
        sum1 -= v1[0] * tmp0 + v1[1] * tmp1; v1 += 2;
        sum2 -= v2[0] * tmp0 + v2[1] * tmp1; v2 += 2;
        sum3 -= v3[0] * tmp0 + v3[1] * tmp1; v3 += 2;
        sum4 -= v4[0] * tmp0 + v4[1] * tmp1; v4 += 2;
        sum5 -= v5[0] * tmp0 + v5[1] * tmp1; v5 += 2;
      }
      if (j == nz-1){
        tmp0 = tmps[*vi++];
        sum1 -= *v1++ *tmp0;
        sum2 -= *v2++ *tmp0;
        sum3 -= *v3++ *tmp0;
        sum4 -= *v4++ *tmp0;
        sum5 -= *v5++ *tmp0;
      }

      sum2 -= *v2++ * sum1;
      sum3 -= *v3++ * sum1;
      sum4 -= *v4++ * sum1;
      sum5 -= *v5++ * sum1;
      sum3 -= *v3++ * sum2;
      sum4 -= *v4++ * sum2;
      sum5 -= *v5++ * sum2;
      sum4 -= *v4++ * sum3;
      sum5 -= *v5++ * sum3;
      sum5 -= *v5++ * sum4;
      
      tmp[row ++]=sum1;
      tmp[row ++]=sum2;
      tmp[row ++]=sum3;
      tmp[row ++]=sum4;
      tmp[row ++]=sum5;
      break;
    default:
      SETERRQ(PETSC_ERR_COR,"Node size not yet supported \n");
    }
  }
  /* backward solve the upper triangular */
  for (i=node_max -1 ,row = n-1 ; i>=0; i--){
    nsz = ns[i];
    aii = ai[row+1] -1;
    v1  = aa + aii;
    vi  = aj + aii;
    nz  = aii- ad[row];
    switch (nsz){               /* Each loop in 'case' is unrolled */
    case 1 :
      sum1 = tmp[row];

      for(j=nz ; j>1; j-=2){
        i0   = vi[0];
        i1   = vi[-1];
        vi  -=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[-1] * tmp1; v1 -= 2;
      }
      if (j==1){
        tmp0  = tmps[*vi--];
        sum1 -= *v1-- * tmp0;
      }
      x[*c--] = tmp[row] = sum1*a_a[ad[row]+shift]; row--;
      break;
    case 2 :
      sum1 = tmp[row];
      sum2 = tmp[row -1];
      v2   = aa + ai[row]-1;
      for (j=nz ; j>1; j-=2){
        i0   = vi[0];
        i1   = vi[-1];
        vi  -=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[-1] * tmp1; v1 -= 2;
        sum2 -= v2[0] * tmp0 + v2[-1] * tmp1; v2 -= 2;
      }
      if (j==1){
        tmp0  = tmps[*vi--];
        sum1 -= *v1-- * tmp0;
        sum2 -= *v2-- * tmp0;
      }
      
      tmp0    = x[*c--] = tmp[row] = sum1*a_a[ad[row]+shift]; row--;
      sum2   -= *v2-- * tmp0;
      x[*c--] = tmp[row] = sum2*a_a[ad[row]+shift]; row--;
      break;
    case 3 :
      sum1 = tmp[row];
      sum2 = tmp[row -1];
      sum3 = tmp[row -2];
      v2   = aa + ai[row]-1;
      v3   = aa + ai[row -1]-1;
      for (j=nz ; j>1; j-=2){
        i0   = vi[0];
        i1   = vi[-1];
        vi  -=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[-1] * tmp1; v1 -= 2;
        sum2 -= v2[0] * tmp0 + v2[-1] * tmp1; v2 -= 2;
        sum3 -= v3[0] * tmp0 + v3[-1] * tmp1; v3 -= 2;
      }
      if (j==1){
        tmp0  = tmps[*vi--];
        sum1 -= *v1-- * tmp0;
        sum2 -= *v2-- * tmp0;
        sum3 -= *v3-- * tmp0;
      }
      tmp0    = x[*c--] = tmp[row] = sum1*a_a[ad[row]+shift]; row--;
      sum2   -= *v2-- * tmp0;
      sum3   -= *v3-- * tmp0;
      tmp0    = x[*c--] = tmp[row] = sum2*a_a[ad[row]+shift]; row--;
      sum3   -= *v3-- * tmp0;
      x[*c--] = tmp[row] = sum3*a_a[ad[row]+shift]; row--;
      
      break;
    case 4 :
      sum1 = tmp[row];
      sum2 = tmp[row -1];
      sum3 = tmp[row -2];
      sum4 = tmp[row -3];
      v2   = aa + ai[row]-1;
      v3   = aa + ai[row -1]-1;
      v4   = aa + ai[row -2]-1;

      for (j=nz ; j>1; j-=2){
        i0   = vi[0];
        i1   = vi[-1];
        vi  -=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[-1] * tmp1; v1 -= 2;
        sum2 -= v2[0] * tmp0 + v2[-1] * tmp1; v2 -= 2;
        sum3 -= v3[0] * tmp0 + v3[-1] * tmp1; v3 -= 2;
        sum4 -= v4[0] * tmp0 + v4[-1] * tmp1; v4 -= 2;
      }
      if (j==1){
        tmp0  = tmps[*vi--];
        sum1 -= *v1-- * tmp0;
        sum2 -= *v2-- * tmp0;
        sum3 -= *v3-- * tmp0;
        sum4 -= *v4-- * tmp0;
      }

      tmp0    = x[*c--] = tmp[row] = sum1*a_a[ad[row]+shift]; row--;
      sum2   -= *v2-- * tmp0;
      sum3   -= *v3-- * tmp0;
      sum4   -= *v4-- * tmp0;
      tmp0    = x[*c--] = tmp[row] = sum2*a_a[ad[row]+shift]; row--;
      sum3   -= *v3-- * tmp0;
      sum4   -= *v4-- * tmp0;
      tmp0    = x[*c--] = tmp[row] = sum3*a_a[ad[row]+shift]; row--;
      sum4   -= *v4-- * tmp0;
      x[*c--] = tmp[row] = sum4*a_a[ad[row]+shift]; row--;
      break;
    case 5 :
      sum1 = tmp[row];
      sum2 = tmp[row -1];
      sum3 = tmp[row -2];
      sum4 = tmp[row -3];
      sum5 = tmp[row -4];
      v2   = aa + ai[row]-1;
      v3   = aa + ai[row -1]-1;
      v4   = aa + ai[row -2]-1;
      v5   = aa + ai[row -3]-1;
      for (j=nz ; j>1; j-=2){
        i0   = vi[0];
        i1   = vi[-1];
        vi  -=2;
        tmp0 = tmps[i0];
        tmp1 = tmps[i1];
        sum1 -= v1[0] * tmp0 + v1[-1] * tmp1; v1 -= 2;
        sum2 -= v2[0] * tmp0 + v2[-1] * tmp1; v2 -= 2;
        sum3 -= v3[0] * tmp0 + v3[-1] * tmp1; v3 -= 2;
        sum4 -= v4[0] * tmp0 + v4[-1] * tmp1; v4 -= 2;
        sum5 -= v5[0] * tmp0 + v5[-1] * tmp1; v5 -= 2;
      }
      if (j==1){
        tmp0  = tmps[*vi--];
        sum1 -= *v1-- * tmp0;
        sum2 -= *v2-- * tmp0;
        sum3 -= *v3-- * tmp0;
        sum4 -= *v4-- * tmp0;
        sum5 -= *v5-- * tmp0;
      }

      tmp0    = x[*c--] = tmp[row] = sum1*a_a[ad[row]+shift]; row--;
      sum2   -= *v2-- * tmp0;
      sum3   -= *v3-- * tmp0;
      sum4   -= *v4-- * tmp0;
      sum5   -= *v5-- * tmp0;
      tmp0    = x[*c--] = tmp[row] = sum2*a_a[ad[row]+shift]; row--;
      sum3   -= *v3-- * tmp0;
      sum4   -= *v4-- * tmp0;
      sum5   -= *v5-- * tmp0;
      tmp0    = x[*c--] = tmp[row] = sum3*a_a[ad[row]+shift]; row--;
      sum4   -= *v4-- * tmp0;
      sum5   -= *v5-- * tmp0;
      tmp0    = x[*c--] = tmp[row] = sum4*a_a[ad[row]+shift]; row--;
      sum5   -= *v5-- * tmp0;
      x[*c--] = tmp[row] = sum5*a_a[ad[row]+shift]; row--;
      break;
    default:
      SETERRQ(PETSC_ERR_COR,"Node size not yet supported \n");
    }
  }
  ierr = ISRestoreIndices(isrow,&rout);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&cout);CHKERRQ(ierr);
  ierr = VecRestoreArray(bb,&b);CHKERRQ(ierr);
  ierr = VecRestoreArray(xx,&x);CHKERRQ(ierr);
  PetscLogFlops(2*a->nz - A->n);
  PetscFunctionReturn(0);
}


#undef __FUNCT__  
#define __FUNCT__ "MatLUFactorNumeric_SeqAIJ_Inode"
int MatLUFactorNumeric_SeqAIJ_Inode(Mat A,Mat *B)
{
  Mat        C = *B;
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data,*b = (Mat_SeqAIJ *)C->data;
  IS         iscol = b->col,isrow = b->row,isicol = b->icol;
  int        shift = a->indexshift,*r,*ic,*c,ierr,n = A->m,*bi = b->i; 
  int        *bj = b->j+shift,*nbj=b->j +(!shift),*ajtmp,*bjtmp,nz,row,prow;
  int        *ics,i,j,idx,*ai = a->i,*aj = a->j+shift,*bd = b->diag,node_max,nsz;
  int        *ns,*tmp_vec1,*tmp_vec2,*nsmap,*pj,ndamp = 0;
  Scalar     *rtmp1,*rtmp2,*rtmp3,*v1,*v2,*v3,*pc1,*pc2,*pc3,mul1,mul2,mul3;
  Scalar     tmp,*ba = b->a+shift,*aa = a->a+shift,*pv,*rtmps1,*rtmps2,*rtmps3;
  PetscTruth damp;
  PetscReal  damping = b->lu_damping;

  PetscFunctionBegin;  
  ierr   = ISGetIndices(isrow,&r);CHKERRQ(ierr);
  ierr   = ISGetIndices(iscol,&c);CHKERRQ(ierr);
  ierr   = ISGetIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = PetscMalloc((3*n+1)*sizeof(Scalar),&rtmp1);CHKERRQ(ierr);
  ierr   = PetscMemzero(rtmp1,(3*n+1)*sizeof(Scalar));CHKERRQ(ierr);
  ics    = ic + shift; rtmps1 = rtmp1 + shift; 
  rtmp2  = rtmp1 + n;  rtmps2 = rtmp2 + shift; 
  rtmp3  = rtmp2 + n;  rtmps3 = rtmp3 + shift; 
  
  node_max = a->inode.node_count; 
  ns       = a->inode.size ;
  if (!ns){                   
    SETERRQ(1,"Matrix without inode information");
  }

  /* If max inode size > 3, split it into two inodes.*/
  /* also map the inode sizes according to the ordering */
  ierr = PetscMalloc((n+1)* sizeof(int),&tmp_vec1);CHKERRQ(ierr);
  for (i=0,j=0; i<node_max; ++i,++j){
    if (ns[i]>3) {
      tmp_vec1[j] = ns[i]/2; /* Assuming ns[i] < =5  */
      ++j; 
      tmp_vec1[j] = ns[i] - tmp_vec1[j-1];
    } else {
      tmp_vec1[j] = ns[i];
    }
  }
  /* Use the correct node_max */
  node_max = j;

  /* Now reorder the inode info based on mat re-ordering info */
  /* First create a row -> inode_size_array_index map */
  ierr = PetscMalloc(n*sizeof(int)+1,&nsmap);CHKERRQ(ierr);
  ierr = PetscMalloc(node_max*sizeof(int)+1,&tmp_vec2);CHKERRQ(ierr);
  for (i=0,row=0; i<node_max; i++) {
    nsz = tmp_vec1[i];
    for (j=0; j<nsz; j++,row++) {
      nsmap[row] = i;
    }
  }
  /* Using nsmap, create a reordered ns structure */
  for (i=0,j=0; i< node_max; i++) {
    nsz       = tmp_vec1[nsmap[r[j]]];    /* here the reordered row_no is in r[] */
    tmp_vec2[i] = nsz;
    j        += nsz;
  }
  ierr = PetscFree(nsmap);CHKERRQ(ierr);
  ierr = PetscFree(tmp_vec1);CHKERRQ(ierr);
  /* Now use the correct ns */
  ns = tmp_vec2;


  do {
    damp = PETSC_FALSE;
    /* Now loop over each block-row, and do the factorization */
    for (i=0,row=0; i<node_max; i++) { 
      nsz   = ns[i];
      nz    = bi[row+1] - bi[row];
      bjtmp = bj + bi[row];
    
      switch (nsz){
      case 1:
        for  (j=0; j<nz; j++){
          idx         = bjtmp[j];
          rtmps1[idx] = 0.0;
        }
      
        /* load in initial (unfactored row) */
        idx   = r[row];
        nz    = ai[idx+1] - ai[idx];
        ajtmp = aj + ai[idx];
        v1    = aa + ai[idx];

        for (j=0; j<nz; j++) {
          idx        = ics[ajtmp[j]];
          rtmp1[idx] = v1[j];
          if (ajtmp[j] == r[row]) {
            rtmp1[idx] += damping;
          }          
        }
        prow = *bjtmp++ + shift;
        while (prow < row) {
          pc1 = rtmp1 + prow;
          if (*pc1 != 0.0){
            pv   = ba + bd[prow];
            pj   = nbj + bd[prow];
            mul1 = *pc1 * *pv++;
            *pc1 = mul1;
            nz   = bi[prow+1] - bd[prow] - 1;
            PetscLogFlops(2*nz);
            for (j=0; j<nz; j++) {
              tmp = pv[j];
              idx = pj[j];
              rtmps1[idx] -= mul1 * tmp;
            }
          }
          prow = *bjtmp++ + shift;
        }
        nz  = bi[row+1] - bi[row];
        pj  = bj + bi[row];
        pc1 = ba + bi[row];
        if (PetscAbsScalar(rtmp1[row]) < 1.e-12) {
          if (b->lu_damping) {
            damp = PETSC_TRUE;
            if (damping) damping *= 2.0;
            else damping = 1.e-12;
            ndamp++;
            goto endofwhile;
          } else {
            SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
          }
        }
        rtmp1[row] = 1.0/rtmp1[row];
        for (j=0; j<nz; j++) {
          idx    = pj[j];
          pc1[j] = rtmps1[idx];
        }
        break;
      
      case 2:
        for  (j=0; j<nz; j++) {
          idx         = bjtmp[j];
          rtmps1[idx] = 0.0;
          rtmps2[idx] = 0.0;
        }
      
        /* load in initial (unfactored row) */
        idx   = r[row];
        nz    = ai[idx+1] - ai[idx];
        ajtmp = aj + ai[idx];
        v1    = aa + ai[idx];
        v2    = aa + ai[idx+1];
      
        for (j=0; j<nz; j++) {
          idx        = ics[ajtmp[j]];
          rtmp1[idx] = v1[j];
          rtmp2[idx] = v2[j];
          if (ajtmp[j] == r[row]) {
            rtmp1[idx] += damping;
          }
          if (ajtmp[j] == r[row+1]) {
            rtmp2[idx] += damping;
          }
        }
        prow = *bjtmp++ + shift;
        while (prow < row) {
          pc1 = rtmp1 + prow;
          pc2 = rtmp2 + prow;
          if (*pc1 != 0.0 || *pc2 != 0.0){
            pv   = ba + bd[prow];
            pj   = nbj + bd[prow];
            mul1 = *pc1 * *pv;
            mul2 = *pc2 * *pv;
            ++pv;
            *pc1 = mul1;
            *pc2 = mul2;
          
            nz   = bi[prow+1] - bd[prow] - 1;
            PetscLogFlops(2*2*nz);
            for (j=0; j<nz; j++) {
              tmp = pv[j];
              idx = pj[j];
              rtmps1[idx] -= mul1 * tmp;
              rtmps2[idx] -= mul2 * tmp;
            }
          }
          prow = *bjtmp++ + shift;
        }
        /* Now take care of the odd element*/
        pc1 = rtmp1 + prow;
        pc2 = rtmp2 + prow;
        if (*pc2 != 0.0){
          pj   = nbj + bd[prow];
          if (PetscAbsScalar(*pc1) < 1.e-12) {
            if (b->lu_damping) {
              damp = PETSC_TRUE;
              if (damping) damping *= 2.0;
              else damping = 1.e-12;
              ndamp++;
              goto endofwhile;
            } else {
              SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
            }
          }
          mul2 = (*pc2)/(*pc1); /* since diag is not yet inverted.*/
          *pc2 = mul2;
          nz   = bi[prow+1] - bd[prow] - 1;
          PetscLogFlops(2*nz);
          for (j=0; j<nz; j++) {
            idx = pj[j] + shift;
            tmp = rtmp1[idx];
            rtmp2[idx] -= mul2 * tmp;
          }
        }
 
        nz  = bi[row+1] - bi[row];
        pj  = bj + bi[row];
        pc1 = ba + bi[row];
        pc2 = ba + bi[row+1];
        if (PetscAbsScalar(rtmp1[row]) < 1.e-12 || PetscAbsScalar(rtmp2[row+1]) < 1.e-12) {
          if (b->lu_damping) {
            damp = PETSC_TRUE;
            if (damping) damping *= 2.0;
            else damping = 1.e-12;
            ndamp++;
            goto endofwhile;
          } else {
            SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
          }
        }
        rtmp1[row]   = 1.0/rtmp1[row];
        rtmp2[row+1] = 1.0/rtmp2[row+1];
        for (j=0; j<nz; j++) {
          idx    = pj[j];
          pc1[j] = rtmps1[idx];
          pc2[j] = rtmps2[idx];
        }
        break;

      case 3:
        for  (j=0; j<nz; j++) {
          idx         = bjtmp[j];
          rtmps1[idx] = 0.0;
          rtmps2[idx] = 0.0;
          rtmps3[idx] = 0.0;
        }
        /* copy the nonzeros for the 3 rows from sparse representation to dense in rtmp*[] */
        idx   = r[row];
        nz    = ai[idx+1] - ai[idx];
        ajtmp = aj + ai[idx];
        v1    = aa + ai[idx];
        v2    = aa + ai[idx+1];
        v3    = aa + ai[idx+2];
        for (j=0; j<nz; j++) {
          idx        = ics[ajtmp[j]];
          rtmp1[idx] = v1[j];
          rtmp2[idx] = v2[j];
          rtmp3[idx] = v3[j];
          if (ajtmp[j] == r[row]) {
            rtmp1[idx] += damping;
          }
          if (ajtmp[j] == r[row+1]) {
            rtmp2[idx] += damping;
          }
          if (ajtmp[j] == r[row+2]) {
            rtmp3[idx] += damping;
          }
        }
        /* loop over all pivot row blocks above this row block */
        prow = *bjtmp++ + shift;
        while (prow < row) {
          pc1 = rtmp1 + prow;
          pc2 = rtmp2 + prow;
          pc3 = rtmp3 + prow;
          if (*pc1 != 0.0 || *pc2 != 0.0 || *pc3 !=0.0){
            pv   = ba  + bd[prow];
            pj   = nbj + bd[prow];
            mul1 = *pc1 * *pv;
            mul2 = *pc2 * *pv; 
            mul3 = *pc3 * *pv;
            ++pv;
            *pc1 = mul1;
            *pc2 = mul2;
            *pc3 = mul3;
          
            nz   = bi[prow+1] - bd[prow] - 1;
            PetscLogFlops(3*2*nz);
            /* update this row based on pivot row */
            for (j=0; j<nz; j++) {
              tmp = pv[j];
              idx = pj[j];
              rtmps1[idx] -= mul1 * tmp;
              rtmps2[idx] -= mul2 * tmp;
              rtmps3[idx] -= mul3 * tmp;
            }
          }
          prow = *bjtmp++ + shift;
        }
        /* Now take care of diagonal block in this set of rows */
        pc1 = rtmp1 + prow;
        pc2 = rtmp2 + prow;
        pc3 = rtmp3 + prow;
        if (*pc2 != 0.0 || *pc3 != 0.0){
          pj   = nbj + bd[prow];
          if (PetscAbsScalar(*pc1) < 1.e-12) {
            if (b->lu_damping) {
              damp = PETSC_TRUE;
              if (damping) damping *= 2.0;
              else damping = 1.e-12;
              ndamp++;
              goto endofwhile;
            } else {
              SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
            }
          }
          mul2 = (*pc2)/(*pc1);
          mul3 = (*pc3)/(*pc1);
          *pc2 = mul2;
          *pc3 = mul3;
          nz   = bi[prow+1] - bd[prow] - 1;
          PetscLogFlops(2*2*nz);
          for (j=0; j<nz; j++) {
            idx = pj[j] + shift;
            tmp = rtmp1[idx];
            rtmp2[idx] -= mul2 * tmp;
            rtmp3[idx] -= mul3 * tmp;
          }
        }
        ++prow;
        pc2 = rtmp2 + prow;
        pc3 = rtmp3 + prow;
        if (*pc3 != 0.0){
          pj   = nbj + bd[prow];
          pj   = nbj + bd[prow];
          if (PetscAbsScalar(*pc2) < 1.e-12) {
            if (b->lu_damping) {
              damp = PETSC_TRUE;
              if (damping) damping *= 2.0;
              else damping = 1.e-12;
              ndamp++;
              goto endofwhile;
            } else {
              SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
            }
          }
          mul3 = (*pc3)/(*pc2);
          *pc3 = mul3;
          nz   = bi[prow+1] - bd[prow] - 1;
          PetscLogFlops(2*2*nz);
          for (j=0; j<nz; j++) {
            idx = pj[j] + shift;
            tmp = rtmp2[idx];
            rtmp3[idx] -= mul3 * tmp;
          }
        }
        nz  = bi[row+1] - bi[row];
        pj  = bj + bi[row];
        pc1 = ba + bi[row];
        pc2 = ba + bi[row+1];
        pc3 = ba + bi[row+2];
        if (PetscAbsScalar(rtmp1[row]) < 1.e-12 || PetscAbsScalar(rtmp2[row+1]) < 1.e-12 || PetscAbsScalar(rtmp3[row+2]) < 1.e-12) {
          if (b->lu_damping) {
            damp = PETSC_TRUE;
            if (damping) damping *= 2.0;
            else damping = 1.e-12;
            ndamp++;
            goto endofwhile;
          } else {
            SETERRQ(PETSC_ERR_MAT_LU_ZRPVT,"Zero pivot");
          }
        }
        rtmp1[row]   = 1.0/rtmp1[row];
        rtmp2[row+1] = 1.0/rtmp2[row+1];
        rtmp3[row+2] = 1.0/rtmp3[row+2];
        /* copy row entries from dense representation to sparse */
        for (j=0; j<nz; j++) {
          idx    = pj[j];
          pc1[j] = rtmps1[idx];
          pc2[j] = rtmps2[idx];
          pc3[j] = rtmps3[idx];
        }
        break;

      default:
        SETERRQ(PETSC_ERR_COR,"Node size not yet supported \n");
      }
      row += nsz;                 /* Update the row */
    } 
    endofwhile:;
  } while (damp);
  ierr = PetscFree(rtmp1);CHKERRQ(ierr);
  ierr = PetscFree(tmp_vec2);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isicol,&ic);CHKERRQ(ierr);
  ierr = ISRestoreIndices(isrow,&r);CHKERRQ(ierr);
  ierr = ISRestoreIndices(iscol,&c);CHKERRQ(ierr);
  C->factor      = FACTOR_LU;
  C->assembled   = PETSC_TRUE;
  if (ndamp || b->lu_damping) {
    PetscLogInfo(0,"MatLUFactorNumerical_SeqAIJ_Inode: number of damping tries %d damping value %g\n",ndamp,damping);
  }
  PetscLogFlops(C->n);
  PetscFunctionReturn(0);
}

/*
     This is really ugly. if inodes are used this replaces the 
  permutations with ones that correspond to rows/cols of the matrix
  rather then inode blocks
*/
#undef __FUNCT__  
#define __FUNCT__ "MatAdjustForInodes"
int MatAdjustForInodes(Mat A,IS *rperm,IS *cperm)
{
  Mat_SeqAIJ *a = (Mat_SeqAIJ*)A->data;
  int        ierr,m = A->m,n = A->n,i,j,*ridx,*cidx,nslim_row = a->inode.node_count;
  int        row,col,*permr,*permc,*ns_row =  a->inode.size,*tns,start_val,end_val,indx;
  int        nslim_col,*ns_col;
  IS         ris = *rperm,cis = *cperm;
  PetscTruth flg;

  PetscFunctionBegin;  
  ierr = PetscTypeCompare((PetscObject)A,MATSEQAIJ,&flg);CHKERRQ(ierr);
  if (!flg) PetscFunctionReturn(0);

  if (!a->inode.size) PetscFunctionReturn(0); /* no inodes so return */
  if (a->inode.node_count == m) PetscFunctionReturn(0); /* all inodes are of size 1 */

  ierr  = Mat_AIJ_CreateColInode(A,&nslim_col,&ns_col);CHKERRQ(ierr);
  ierr  = PetscMalloc((((nslim_row>nslim_col)?nslim_row:nslim_col)+1)*sizeof(int),&tns);CHKERRQ(ierr);
  ierr  = PetscMalloc((m+n+1)*sizeof(int),&permr);CHKERRQ(ierr);
  permc = permr + m;

  ierr  = ISGetIndices(ris,&ridx);CHKERRQ(ierr);
  ierr  = ISGetIndices(cis,&cidx);CHKERRQ(ierr);

  /* Form the inode structure for the rows of permuted matric using inv perm*/
  for (i=0,tns[0]=0; i<nslim_row; ++i) tns[i+1] = tns[i] + ns_row[i];

  /* Construct the permutations for rows*/
  for (i=0,row = 0; i<nslim_row; ++i){
    indx      = ridx[i];
    start_val = tns[indx];
    end_val   = tns[indx + 1];
    for (j=start_val; j<end_val; ++j,++row) permr[row]= j;
  }

  /* Form the inode structure for the columns of permuted matrix using inv perm*/
  for (i=0,tns[0]=0; i<nslim_col; ++i) tns[i+1] = tns[i] + ns_col[i];

 /*Construct permutations for columns*/
  for (i=0,col=0; i<nslim_col; ++i){
    indx      = cidx[i];
    start_val = tns[indx];
    end_val   = tns[indx + 1];
    for (j = start_val; j<end_val; ++j,++col) permc[col]= j;
  }

  ierr = ISCreateGeneral(PETSC_COMM_SELF,n,permr,rperm);CHKERRQ(ierr);
  ISSetPermutation(*rperm);
  ierr = ISCreateGeneral(PETSC_COMM_SELF,n,permc,cperm);CHKERRQ(ierr);
  ISSetPermutation(*cperm);
 
  ierr  = ISRestoreIndices(ris,&ridx);CHKERRQ(ierr);
  ierr  = ISRestoreIndices(cis,&cidx);CHKERRQ(ierr);

  ierr = PetscFree(ns_col);CHKERRQ(ierr);
  ierr = PetscFree(permr);CHKERRQ(ierr);
  ierr = ISDestroy(cis);CHKERRQ(ierr);
  ierr = ISDestroy(ris);CHKERRQ(ierr);
  ierr = PetscFree(tns);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
}


#undef __FUNCT__  
#define __FUNCT__ "MatSeqAIJGetInodeSizes"

/*@C
   MatSeqAIJGetInodeSizes - Returns the inode information of the SeqAIJ matrix.

   Collective on Mat

   Input Parameter:
.  A - the SeqAIJ matrix.

   Output Parameter:
+  node_count - no of inodes present in the matrix.
.  sizes      - an array of size node_count,with sizes of each inode.
-  limit      - the max size used to generate the inodes.

   Level: advanced

   Notes: This routine returns some internal storage information
   of the matrix, it is intended to be used by advanced users.
   It should be called after the matrix is assembled.
   The contents of the sizes[] array should not be changed.

.keywords: matrix, seqaij, get, inode

.seealso: MatGetInfo()
@*/
int MatSeqAIJGetInodeSizes(Mat A,int *node_count,int *sizes[],int *limit)
{
  Mat_SeqAIJ *a;
  PetscTruth flg;
  int        ierr;

  PetscFunctionBegin;  
  PetscValidHeaderSpecific(A,MAT_COOKIE);
  ierr = PetscTypeCompare((PetscObject)A,MATSEQAIJ,&flg);CHKERRQ(ierr);
  if (!flg) SETERRQ(PETSC_ERR_ARG_WRONG,"MatSeqAIJ only");
  a           = (Mat_SeqAIJ*)A->data; 
  *node_count = a->inode.node_count;
  *sizes      = a->inode.size;
  *limit      = a->inode.limit;
  PetscFunctionReturn(0);
}
