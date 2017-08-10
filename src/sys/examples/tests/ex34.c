static char help[] = "Tests IsInf/IsNan routines.\n";

#include <petscsys.h>

static PetscBool PetscIsInfRealFallback(volatile PetscReal a)
{
  return (a && a/2 == a) ? PETSC_TRUE : PETSC_FALSE;
}

static PetscBool PetscIsNanRealFallback(volatile PetscReal a)
{
  return (a != a) ? PETSC_TRUE : PETSC_FALSE;
}

static PetscReal PetscRealFromFloatBits(unsigned int bits) {
  union {unsigned int ival; float fval;} u;
  u.ival = bits;
  return u.fval;
}

#define CALL(call) do { \
    PetscErrorCode _ierr;                                               \
    _ierr = PetscPrintf(PETSC_COMM_WORLD,"%-32s -> %s\n",#call,(call)?"True":"False");CHKERRQ(_ierr); \
  } while(0);

int main(int argc, char **argv) {

  PetscReal neg_zero = PetscRealConstant(-0.0);
  PetscReal pos_zero = PetscRealConstant(+0.0);
  PetscReal neg_one  = PetscRealConstant(-1.0);
  PetscReal pos_one  = PetscRealConstant(+1.0);
  PetscReal neg_inf  = PetscRealFromFloatBits(0xFF800000); /* negative infinity */
  PetscReal pos_inf  = PetscRealFromFloatBits(0x7F800000); /* positive infinity */
  PetscReal s_nan    = PetscRealFromFloatBits(0xFFBFFFFF); /* signaling NaN */
  PetscReal q_nan    = PetscRealFromFloatBits(0xFFFFFFFF); /* quiet NaN */

  PetscErrorCode ierr;
  ierr = PetscInitialize(&argc,&argv,NULL,help);if (ierr) return ierr;

  CALL(PetscIsInfReal(neg_zero));
  CALL(PetscIsInfReal(pos_zero));
  CALL(PetscIsInfReal(neg_one));
  CALL(PetscIsInfReal(pos_one));
  CALL(PetscIsInfReal(neg_inf));
  CALL(PetscIsInfReal(pos_inf));
  CALL(PetscIsInfReal(s_nan));
  CALL(PetscIsInfReal(q_nan));

  CALL(PetscIsNanReal(neg_zero));
  CALL(PetscIsNanReal(pos_zero));
  CALL(PetscIsNanReal(neg_one));
  CALL(PetscIsNanReal(pos_one));
  CALL(PetscIsNanReal(neg_inf));
  CALL(PetscIsNanReal(pos_inf));
  CALL(PetscIsNanReal(s_nan));
  CALL(PetscIsNanReal(q_nan));

  CALL(PetscIsInfRealFallback(neg_zero));
  CALL(PetscIsInfRealFallback(pos_zero));
  CALL(PetscIsInfRealFallback(neg_one));
  CALL(PetscIsInfRealFallback(pos_one));
  CALL(PetscIsInfRealFallback(neg_inf));
  CALL(PetscIsInfRealFallback(pos_inf));
  CALL(PetscIsInfRealFallback(s_nan));
  CALL(PetscIsInfRealFallback(q_nan));

  CALL(PetscIsNanRealFallback(neg_zero));
  CALL(PetscIsNanRealFallback(pos_zero));
  CALL(PetscIsNanRealFallback(neg_one));
  CALL(PetscIsNanRealFallback(pos_one));
  CALL(PetscIsNanRealFallback(neg_inf));
  CALL(PetscIsNanRealFallback(pos_inf));
  CALL(PetscIsNanRealFallback(s_nan));
  CALL(PetscIsNanRealFallback(q_nan));

  ierr = PetscFinalize();
  return ierr;
}


/*TEST

   test:
      output_file: output/ex34.out

TEST*/
