
static char help[] = 
"This example builds a parallel vector with 1 component on the first\n\
processor, 2 on the second, etc.  Then each processor adds one to all\n\
elements except the last mytid.\n\n";

#include "petsc.h"
#include "is.h"
#include "vec.h"
#include "sys.h"
#include "sysio.h"
#include <math.h>

int main(int argc,char **argv)
{
  int          i,N,ierr, numtids,mytid;
  Scalar       one = 1.0;
  Vec          x;

  PetscInitialize(&argc,&argv,0,0);
  if (OptionsHasName(0,"-help")) fprintf(stderr,"%s",help);
  MPI_Comm_size(MPI_COMM_WORLD,&numtids);
  MPI_Comm_rank(MPI_COMM_WORLD,&mytid); 

  ierr = VecCreateMPI(MPI_COMM_WORLD,mytid+1,PETSC_DECIDE,&x); CHKERRA(ierr);
  ierr = VecGetSize(x,&N); CHKERRA(ierr);
  ierr = VecSet(&one,x); CHKERRA(ierr);

  for ( i=0; i<N-mytid; i++ ) {
    ierr = VecSetValues(x,1,&i,&one,ADDVALUES); CHKERRA(ierr);  
  }
  ierr = VecAssemblyBegin(x); CHKERRA(ierr);
  ierr = VecAssemblyEnd(x); CHKERRA(ierr);

  ierr = VecView(x,SYNC_STDOUT_VIEWER); CHKERRA(ierr);

  ierr = VecDestroy(x); CHKERRA(ierr);

  PetscFinalize();
  return 0;
}
 
