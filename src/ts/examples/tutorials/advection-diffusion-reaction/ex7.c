
static char help[] = ".\n";

/*

        u_t =  u_xx + R(u)

      Where u(t,x,i)    for i=1, .... N where i represents the void size 
*/

#include <petscdmda.h>
#include <petscts.h>

/*
   User-defined data structures and routines
*/

/* AppCtx */
typedef struct {
  PetscInt  N;              /* number of dofs */
  PetscBool viewJacobian;
} AppCtx;

extern PetscErrorCode IFunction(TS,PetscReal,Vec,Vec,Vec,void*);
extern PetscErrorCode InitialConditions(DM,Vec);
extern PetscErrorCode IJacobian(TS,PetscReal,Vec,Vec,PetscReal,Mat*,Mat*,MatStructure*,void*);


#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  TS              ts;                 /* nonlinear solver */
  Vec             U;                  /* solution, residual vectors */
  Mat             J;                  /* Jacobian matrix */
  PetscInt        maxsteps = 1000;
  PetscErrorCode  ierr;
  DM              da;
  AppCtx          user;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscInitialize(&argc,&argv,(char *)0,help);
  user.viewJacobian = PETSC_FALSE;
  user.N            = 1;
  ierr = PetscOptionsGetInt(PETSC_NULL,"-N",&user.N,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-viewJacobian",&user.viewJacobian);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DMDA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMDACreate1d(PETSC_COMM_WORLD, DMDA_BOUNDARY_NONE,-8,user.N,1,PETSC_NULL,&da);CHKERRQ(ierr);

  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   Extract global vectors from DMDA; then duplicate for remaining
     vectors that are the same types
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMCreateGlobalVector(da,&U);CHKERRQ(ierr);
  ierr = DMCreateMatrix(da,MATAIJ,&J);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSCreate(PETSC_COMM_WORLD,&ts);CHKERRQ(ierr);
  ierr = TSSetType(ts,TSARKIMEX);CHKERRQ(ierr);
  ierr = TSSetDM(ts,da);CHKERRQ(ierr);
  ierr = TSSetProblemType(ts,TS_NONLINEAR);CHKERRQ(ierr);
  ierr = TSSetIFunction(ts,PETSC_NULL,IFunction,&user);CHKERRQ(ierr);
  ierr = TSSetIJacobian(ts,J,J,IJacobian,&user);CHKERRQ(ierr);


  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = InitialConditions(da,U);CHKERRQ(ierr);
  ierr = TSSetSolution(ts,U);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set solver options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSetInitialTimeStep(ts,0.0,.001);CHKERRQ(ierr);
  ierr = TSSetDuration(ts,maxsteps,1.0);CHKERRQ(ierr);
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSolve(ts,U);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = VecDestroy(&U);CHKERRQ(ierr);
  ierr = TSDestroy(&ts);CHKERRQ(ierr);
  ierr = DMDestroy(&da);CHKERRQ(ierr);

  ierr = PetscFinalize();
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "IFunction"
/*
   IFunction - Evaluates nonlinear function, F(U).

   Input Parameters:
.  ts - the TS context
.  U - input vector
.  ptr - optional user-defined context, as set by SNESSetFunction()

   Output Parameter:
.  F - function vector
 */
PetscErrorCode IFunction(TS ts,PetscReal ftime,Vec U,Vec Udot,Vec F,void *ptr)
{
  DM             da;
  PetscErrorCode ierr;
  PetscInt       i,j,Mx,xs,xm,N;
  PetscReal      hx,sx,x;
  PetscScalar    uxx;
  PetscScalar    **u,**f,**udot;
  Vec            localU;

  PetscFunctionBegin;
  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMGetLocalVector(da,&localU);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,&N,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);CHKERRQ(ierr);

  hx     = 1.0/(PetscReal)(Mx-1); sx = 1.0/(hx*hx);

  /*
     Scatter ghost points to local vector,using the 2-step process
        DMGlobalToLocalBegin(),DMGlobalToLocalEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  ierr = DMGlobalToLocalBegin(da,U,INSERT_VALUES,localU);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(da,U,INSERT_VALUES,localU);CHKERRQ(ierr);

  /*
     Get pointers to vector data
  */
  ierr = DMDAVecGetArrayDOF(da,localU,&u);CHKERRQ(ierr);
  ierr = DMDAVecGetArrayDOF(da,Udot,&udot);CHKERRQ(ierr);
  ierr = DMDAVecGetArrayDOF(da,F,&f);CHKERRQ(ierr);
 
  /*
     Get local grid boundaries
  */
  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  /* 
     Neumann BC
  */
  if (!xs) {
    for (i=0; i<N; i++) {
      f[0][i] = udot[0][i] + 2.0*(u[0][i] - u[1][i])*sx;
    }
    xs++;
    xm--;
  }
  if (xs+xm == Mx) {
    for (i=0; i<N; i++) {
      f[Mx-1][i] = udot[Mx-1][i] +  2.0*(u[Mx-1][i] - u[Mx-2][i])*sx;
    }
    xm--;
  }

  /*
     Compute function over the locally owned part of the grid
  */
  for (j=xs; j<xs+xm; j++) {
    x = i*hx;

    /*  diffusion term */
    for (i=0; i<N; i++) {
      uxx      = (-2.0*u[j][i] + u[j-1][i] + u[j+1][i])*sx;
      f[j][i]   = udot[j][i] - uxx;
    }

    /* reaction terms */
    /*
    for (i=0; i<N/3; i++) {
      f[j][i]   += 500*u[j][i]*u[j][i+1];
      f[j][i+1] += 500*u[j][i]*u[j][i+1];
      f[j][i+2] -= 500*u[j][i]*u[j][i+1];
    }
     */

    /* forcing term */
    /*
    f[j][0] -= 5*PetscExpScalar(1.0 - x);
     */
  }

  /*
     Restore vectors
  */
  ierr = DMDAVecRestoreArrayDOF(da,localU,&u);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArrayDOF(da,Udot,&udot);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArrayDOF(da,F,&f);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(da,&localU);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "IJacobian"
PetscErrorCode IJacobian(TS ts,PetscReal t,Vec U,Vec Udot,PetscReal a,Mat *J,Mat *Jpre,MatStructure *str,void *ctx)
{
  PetscErrorCode ierr;
  PetscInt       i,c,Mx,xs,xm,nc;
  DM             da;
  MatStencil     col[3],row;
  PetscScalar    vals[3],hx,sx;
  AppCtx         *user = (AppCtx*)ctx;
  PetscInt       N = user->N;

  PetscFunctionBegin;
  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);
  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  hx = 1.0/(PetscReal)(Mx-1); sx = 1.0/(hx*hx);

  for (c=0; c<N; c++){
    for (i=xs; i<xs+xm; i++){
      nc    = 0;
      row.i = i; row.c = c;

      if (i == 0) {  /* Left Neumann */
        col[nc].c = c; col[nc].i = i;   vals[nc++] = 2.0*sx + a;
        col[nc].c = c; col[nc].i = i+1; vals[nc++] = -2.0*sx;
      } else if (i == Mx-1){/* Right Neumann */
        col[nc].c = c; col[nc].i = i;   vals[nc++] = 2.0*sx + a;
        col[nc].c = c; col[nc].i = i-1; vals[nc++] = -2.0*sx;
      } else {   /* Interior */
        col[nc].c = c; col[nc].i = i-1; vals[nc++] = -sx;
        col[nc].c = c; col[nc].i = i;   vals[nc++] = 2.0*sx + a;
        col[nc].c = c; col[nc].i = i+1; vals[nc++] = -sx;
      }
      ierr = MatSetValuesStencil(*Jpre,1,&row,nc,col,vals,INSERT_VALUES);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(*Jpre,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*Jpre,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  if (*J != *Jpre) {
    ierr = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
    ierr = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  }

  if (user->viewJacobian){
    ierr = PetscPrintf(((PetscObject)*Jpre)->comm,"Jpre:\n");CHKERRQ(ierr);
    ierr = MatView(*Jpre,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}

/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "InitialConditions"
PetscErrorCode InitialConditions(DM da,Vec U)
{
  PetscErrorCode ierr;
  PetscInt       i,j,xs,xm,Mx,N;
  PetscScalar    **u;
  PetscReal      hx,x;

  PetscFunctionBegin;
  ierr = DMDAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,&N,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);CHKERRQ(ierr);

  hx     = 1.0/(PetscReal)(Mx-1);

  /*
     Get pointers to vector data
  */
  ierr = DMDAVecGetArrayDOF(da,U,&u);CHKERRQ(ierr);

  /*
     Get local grid boundaries
  */
  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  /*
     Compute function over the locally owned part of the grid
  */
  for (j=xs; j<xs+xm; j++) {
    x = j*hx;
    for (i=0; i<N; i++) {
      u[j][i] = PetscCosScalar(PETSC_PI*x);
    }
  }

  /*
     Restore vectors
  */
  ierr = DMDAVecRestoreArrayDOF(da,U,&u);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}


