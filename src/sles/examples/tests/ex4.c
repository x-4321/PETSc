
static char help[] = 
"This example solves a linear system with SLES.  The matrix uses simple\n\
bilinear elements on the unit square. Input arguments are:\n\
  -m <size> : problem size\n\n";

#include "petsc.h"
#include "sles.h"
#include  <stdio.h>

int FormElementStiffness(double H,Scalar *Ke)
{
  Ke[0]  = H/6.0;    Ke[1]  = -.125*H; Ke[2]  = H/12.0;   Ke[3]  = -.125*H;
  Ke[4]  = -.125*H;  Ke[5]  = H/6.0;   Ke[6]  = -.125*H;  Ke[7]  = H/12.0;
  Ke[8]  = H/12.0;   Ke[9]  = -.125*H; Ke[10] = H/6.0;    Ke[11] = -.125*H;
  Ke[12] = -.125*H;  Ke[13] = H/12.0;  Ke[14] = -.125*H;  Ke[15] = H/6.0;
  return 0;
}
int FormElementRhs(double x, double y, double H,Scalar *r)
{
  r[0] = 0.; r[1] = 0.; r[2] = 0.; r[3] = 0.0; 
  return 0;
}

int main(int argc,char **args)
{
  Mat         C; 
  int         i, m = 2,  N,M,its;
  Scalar      val, zero = 0.0, one = 1.0, none = -1.0,Ke[16],r[4];
  double      x,y,h,norm;
  int         ierr,idx[4],count,*rows;
  Vec         u,ustar,b;
  SLES        sles;
  KSP         ksp;
  IS          is;

  PetscInitialize(&argc,&args,0,0);
  if (OptionsHasName(0,"-help")) fprintf(stderr,"%s",help);
  OptionsGetInt(0,"-m",&m);
  N = (m+1)*(m+1); /* dimension of matrix */
  M = m*m; /* number of elements */
  h = 1.0/m;       /* mesh width */

  /* create stiffness matrix */
  ierr = MatCreateSequentialAIJ(MPI_COMM_SELF,N,N,9,0,&C); CHKERRA(ierr);

  /* forms the element stiffness for the Laplacian */
  ierr = FormElementStiffness(h*h,Ke); CHKERRA(ierr);
  for ( i=0; i<M; i++ ) {
     /* location of lower left corner of element */
     x = h*(i % m); y = h*(i/m); 
     /* node numbers for the four corners of element */
     idx[0] = (m+1)*(i/m) + ( i % m);
     idx[1] = idx[0]+1; idx[2] = idx[1] + m + 1; idx[3] = idx[2] - 1;
     ierr = MatSetValues(C,4,idx,4,idx,Ke,ADDVALUES); CHKERRA(ierr);
  }
  ierr = MatAssemblyBegin(C,FINAL_ASSEMBLY); CHKERRA(ierr);
  ierr = MatAssemblyEnd(C,FINAL_ASSEMBLY); CHKERRA(ierr);

  /* create right hand side and solution */

  ierr = VecCreateSequential(MPI_COMM_SELF,N,&u); CHKERRA(ierr); 
  ierr = VecDuplicate(u,&b); CHKERRA(ierr);
  ierr = VecDuplicate(b,&ustar); CHKERRA(ierr);
  ierr = VecSet(&zero,u); CHKERRA(ierr);
  ierr = VecSet(&zero,b); CHKERRA(ierr);

  for ( i=0; i<M; i++ ) {
     /* location of lower left corner of element */
     x = h*(i % m); y = h*(i/m); 
     /* node numbers for the four corners of element */
     idx[0] = (m+1)*(i/m) + ( i % m);
     idx[1] = idx[0]+1; idx[2] = idx[1] + m + 1; idx[3] = idx[2] - 1;
     ierr = FormElementRhs(x,y,h*h,r); CHKERRA(ierr);
     ierr = VecSetValues(b,4,idx,r,ADDVALUES); CHKERRA(ierr);
  }
  ierr = VecAssemblyBegin(b); CHKERRA(ierr);
  ierr = VecAssemblyEnd(b); CHKERRA(ierr);

  /* modify matrix and rhs for Dirichlet boundary conditions */
  rows = (int *) MALLOC( 4*m*sizeof(int) ); CHKPTR(rows);
  for ( i=0; i<m+1; i++ ) {
    rows[i] = i; /* bottom */
    rows[3*m - 1 +i] = m*(m+1) + i; /* top */
  }
  count = m+1; /* left side */
  for ( i=m+1; i<m*(m+1); i+= m+1 ) {
    rows[count++] = i;
  }
  count = 2*m; /* left side */
  for ( i=2*m+1; i<m*(m+1); i+= m+1 ) {
    rows[count++] = i;
  }
  ierr = ISCreateSequential(MPI_COMM_SELF,4*m,rows,&is); CHKERRA(ierr);
  for ( i=0; i<4*m; i++ ) {
     x = h*(rows[i] % (m+1)); y = h*(rows[i]/(m+1)); 
     val = y;
     ierr = VecSetValues(u,1,&rows[i],&val,INSERTVALUES); CHKERRA(ierr);
     ierr = VecSetValues(b,1,&rows[i],&val,INSERTVALUES); CHKERRA(ierr);
  }    
  FREE(rows);
  ierr = VecAssemblyBegin(u); CHKERRA(ierr);
  ierr = VecAssemblyEnd(u); CHKERRA(ierr);
  ierr = VecAssemblyBegin(b); CHKERRA(ierr);
  ierr = VecAssemblyEnd(b); CHKERRA(ierr);

  ierr = MatZeroRows(C,is,&one); CHKERRA(ierr);
  ierr = ISDestroy(is); CHKERRA(ierr);

  /* solve linear system */
  ierr = SLESCreate(MPI_COMM_WORLD,&sles); CHKERRA(ierr);
  ierr = SLESSetOperators(sles,C,C,0); CHKERRA(ierr);
  ierr = SLESSetFromOptions(sles); CHKERRA(ierr);
  ierr = SLESGetKSP(sles,&ksp); CHKERRA(ierr);
  ierr = KSPSetInitialGuessNonzero(ksp); CHKERRA(ierr);
  ierr = SLESSolve(sles,b,u,&its); CHKERRA(ierr);

  /* check error */
  for ( i=0; i<N; i++ ) {
     x = h*(i % (m+1)); y = h*(i/(m+1)); 
     val = y;
     ierr = VecSetValues(ustar,1,&i,&val,INSERTVALUES); CHKERRA(ierr);
  }
  ierr = VecAssemblyBegin(ustar); CHKERRA(ierr);
  ierr = VecAssemblyEnd(ustar); CHKERRA(ierr);

  ierr = VecAXPY(&none,ustar,u); CHKERRA(ierr);
  ierr = VecNorm(u,&norm); CHKERRA(ierr);
  MPE_printf(MPI_COMM_WORLD,"Norm of error %g Number iterations %d\n",
      norm*h,its);

  sleep(2);
  ierr = SLESDestroy(sles); CHKERRA(ierr);
  ierr = VecDestroy(ustar); CHKERRA(ierr);
  ierr = VecDestroy(u); CHKERRA(ierr);
  ierr = VecDestroy(b); CHKERRA(ierr);
  ierr = MatDestroy(C); CHKERRA(ierr);
  PetscFinalize();
  return 0;
}
