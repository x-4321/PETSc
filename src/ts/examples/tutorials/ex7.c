/*$Id: ex7.c,v 1.3 2001/04/14 03:58:23 bsmith Exp bsmith $*/

/* Program usage:  mpirun -np <procs> ex5 [-help] [all PETSc options] */

static char help[] = "Nonlinear, time-dependent PDE in 2d.\n";


/* 
   Include "petscda.h" so that we can use distributed arrays (DAs).
   Include "petscts.h" so that we can use SNES solvers.  Note that this
   file automatically includes:
     petsc.h       - base PETSc routines   petscvec.h - vectors
     petscsys.h    - system routines       petscmat.h - matrices
     petscis.h     - index sets            petscksp.h - Krylov subspace methods
     petscviewer.h - viewers               petscpc.h  - preconditioners
     petscsles.h   - linear solvers
*/
#include "petscda.h"
#include "petscts.h"


/* 
   User-defined routines
*/
extern int FormFunction(TS,double,Vec,Vec,void*),FormInitialSolution(DA,Vec);
extern int Monitor(TS,int,double,Vec,void*);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  TS                     ts;                 /* nonlinear solver */
  Vec                    x,r;                  /* solution, residual vectors */
  Mat                    J;                    /* Jacobian matrix */
  int                    steps;                  /* iterations for convergence */
  int                    ierr;
  DA                     da;
  MatFDColoring          matfdcoloring;
  ISColoring             iscoloring;
  double                 ftime;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  PetscInitialize(&argc,&argv,(char *)0,help);


  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DACreate2d(PETSC_COMM_WORLD,DA_NONPERIODIC,DA_STENCIL_STAR,8,8,PETSC_DECIDE,PETSC_DECIDE,
                    1,1,PETSC_NULL,PETSC_NULL,&da);CHKERRQ(ierr);

  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Extract global vectors from DA; then duplicate for remaining
     vectors that are the same types
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DACreateGlobalVector(da,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&r);CHKERRQ(ierr);

  ierr = TSCreate(PETSC_COMM_WORLD,TS_NONLINEAR,&ts);CHKERRQ(ierr);
  ierr = TSSetRHSFunction(ts,FormFunction,da);CHKERRQ(ierr);


  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create matrix data structure; set Jacobian evaluation routine

     Set Jacobian matrix data structure and default Jacobian evaluation
     routine. User can override with:
     -snes_mf : matrix-free Newton-Krylov method with no preconditioning
                (unless user explicitly sets preconditioner) 
     -snes_mf_operator : form preconditioning matrix as set by the user,
                         but use matrix-free approx for Jacobian-vector
                         products within Newton-Krylov method

     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DAGetColoring(da,IS_COLORING_LOCAL,&iscoloring);CHKERRQ(ierr);
  ierr = DAGetMatrix(da,MATMPIAIJ,&J);CHKERRQ(ierr);
  ierr = MatFDColoringCreate(J,iscoloring,&matfdcoloring);CHKERRQ(ierr);
  ierr = ISColoringDestroy(iscoloring);CHKERRQ(ierr);
  ierr = MatFDColoringSetFunction(matfdcoloring,(int (*)(void))FormFunction,da);CHKERRQ(ierr);
  ierr = MatFDColoringSetFromOptions(matfdcoloring);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize nonlinear solver; set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSetInitialTimeStep(ts,0.0,.0001);CHKERRQ(ierr);
  ierr = TSSetType(ts,TS_BEULER);CHKERRQ(ierr);
  ierr = TSSetDuration(ts,100,1.0);CHKERRQ(ierr);
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);
  ierr = TSSetMonitor(ts,Monitor,0,0);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = FormInitialSolution(da,x);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSetSolution(ts,x);CHKERRQ(ierr);
  ierr = TSStep(ts,&steps,&ftime);CHKERRQ(ierr);


  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  ierr = MatDestroy(J);CHKERRQ(ierr);
  ierr = MatFDColoringDestroy(matfdcoloring);CHKERRQ(ierr);
  ierr = VecDestroy(x);CHKERRQ(ierr);
  ierr = VecDestroy(r);CHKERRQ(ierr);      
  ierr = TSDestroy(ts);CHKERRQ(ierr);
  ierr = DADestroy(da);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);

  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "FormFunction"
/* 
   FormFunction - Evaluates nonlinear function, F(x).

   Input Parameters:
.  ts - the TS context
.  X - input vector
.  ptr - optional user-defined context, as set by SNESSetFunction()

   Output Parameter:
.  F - function vector
 */
int FormFunction(TS ts,double time,Vec X,Vec F,void *ptr)
{
  DA      da = (DA)ptr;
  int     ierr,i,j,Mx,My,xs,ys,xm,ym;
  double  two = 2.0,hx,hy,hxdhy,hydhx,sx,sy;
  Scalar  u,uxx,uyy,**x,**f;
  Vec     localX;

  PetscFunctionBegin;
  ierr = DAGetLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = DAGetInfo(da,PETSC_IGNORE,&Mx,&My,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,
                   PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);

  hx     = 1.0/(double)(Mx-1); sx = 1.0/(hx*hx);
  hy     = 1.0/(double)(My-1); sy = 1.0/(hy*hy);
  hxdhy  = hx/hy; 
  hydhx  = hy/hx;

  /*
     Scatter ghost points to local vector,using the 2-step process
        DAGlobalToLocalBegin(),DAGlobalToLocalEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  ierr = DAGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DAGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);

  /*
     Get pointers to vector data
  */
  ierr = DAVecGetArray(da,localX,(void**)&x);CHKERRQ(ierr);
  ierr = DAVecGetArray(da,F,(void**)&f);CHKERRQ(ierr);

  /*
     Get local grid boundaries
  */
  ierr = DAGetCorners(da,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL);CHKERRQ(ierr);

  /*
     Compute function over the locally owned part of the grid
  */
  for (j=ys; j<ys+ym; j++) {
    for (i=xs; i<xs+xm; i++) {
      if (i == 0 || j == 0 || i == Mx-1 || j == My-1) {
        f[j][i] = x[j][i];
        continue;
      }
      u       = x[j][i];
      uxx     = (two*u - x[j][i-1] - x[j][i+1])*sx;
      uyy     = (two*u - x[j-1][i] - x[j+1][i])*sy;
      /*      f[j][i] = -(uxx + uyy); */
      f[j][i] = -u*(uxx + uyy) - (4.0 - 1.0)*((x[j][i+1] - x[j][i-1])*(x[j][i+1] - x[j][i-1])*.25*sx +
                                            (x[j+1][i] - x[j-1][i])*(x[j+1][i] - x[j-1][i])*.25*sy); 
    }
  }

  /*
     Restore vectors
  */
  ierr = DAVecRestoreArray(da,localX,(void**)&x);CHKERRQ(ierr);
  ierr = DAVecRestoreArray(da,F,(void**)&f);CHKERRQ(ierr);
  ierr = DARestoreLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = PetscLogFlops(11*ym*xm);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "FormInitialSolution"
int FormInitialSolution(DA da,Vec U)
{
  int     ierr,i,j,xs,ys,xm,ym,Mx,My;
  Scalar  **u;
  double  hx,hy,x,y,r;

  PetscFunctionBegin;
  ierr = DAGetInfo(da,PETSC_IGNORE,&Mx,&My,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,
                   PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);

  hx     = 1.0/(double)(Mx-1);
  hy     = 1.0/(double)(My-1);

  /*
     Get pointers to vector data
  */
  ierr = DAVecGetArray(da,U,(void**)&u);CHKERRQ(ierr);

  /*
     Get local grid boundaries
  */
  ierr = DAGetCorners(da,&xs,&ys,PETSC_NULL,&xm,&ym,PETSC_NULL);CHKERRQ(ierr);

  /*
     Compute function over the locally owned part of the grid
  */
  for (j=ys; j<ys+ym; j++) {
    y = j*hy;
    for (i=xs; i<xs+xm; i++) {
      x = i*hx;
      r = PetscSqrtScalar((x-.5)*(x-.5) + (y-.5)*(y-.5));
      if (r < .125) {
        u[j][i] = PetscExpScalar(-30.0*r*r*r);
      } else {
        u[j][i] = 0.0;
      }
    }
  }

  /*
     Restore vectors
  */
  ierr = DAVecRestoreArray(da,U,(void**)&u);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

#undef __FUNCT__  
#define __FUNCT__ "Monitor"
int Monitor(TS ts,int step,double ptime,Vec v,void *ctx)
{
  int      ierr;
  double   norm;
  MPI_Comm comm;

  PetscFunctionBegin;
  ierr = VecNorm(v,NORM_2,&norm);CHKERRQ(ierr);
  ierr = PetscObjectGetComm((PetscObject)ts,&comm);CHKERRQ(ierr);
  ierr = PetscPrintf(comm,"timestep %d time %g norm %g\n",step,ptime,norm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
