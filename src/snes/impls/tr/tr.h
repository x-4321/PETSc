/* $Id: tr.h,v 1.2 1995/05/05 03:51:35 bsmith Exp bsmith $ */

/*
   Context for a Newton trust region method (SNES_NTR) 
 */

#ifndef __TR_H
#define __TR_H
#include "snesimpl.h"

typedef struct {
  /* ---- Parameters used by the trust region method  ---- */
  double deltatol;
  double mu;		/* used to compute trust region parameter */
  double eta;		/* used to compute trust region parameter */
  double delta;		/* trust region parameter */
  double delta0;	/* used to initialize trust region parameter */
  double delta1;	/* used to compute trust region parameter */
  double delta2;	/* used to compute trust region parameter */
  double delta3;	/* used to compute trust region parameter */
  double sigma;		/* used to detemine termination */
  int    itflag;	/* flag for convergence testing */
  double rnorm0,ttol;   /* used for KSP convergence test */
} SNES_TR;

#endif
