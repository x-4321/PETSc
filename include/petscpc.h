/* $Id: pc.h,v 1.35 1995/10/12 18:55:35 curfman Exp bsmith $ */

/*
      Preconditioner module.
*/
#if !defined(__PC_PACKAGE)
#define __PC_PACKAGE
#include "petsc.h"
#include "mat.h"

typedef enum { PCNONE, PCJACOBI, PCSOR, PCLU, PCSHELL, PCBJACOBI, PCMG,
               PCEISENSTAT, PCILU, PCICC, PCASM } PCMethod;

typedef struct _PC* PC;
#define PC_COOKIE    PETSC_COOKIE+9

extern int    PCCreate(MPI_Comm,PC*);
extern int    PCSetMethod(PC,PCMethod);
extern int    PCSetUp(PC);
extern int    PCApply(PC,Vec,Vec);
extern int    PCApplyBAorAB(PC,int,Vec,Vec,Vec);
extern int    PCApplyTrans(PC,Vec,Vec);
extern int    PCApplyBAorABTrans(PC,int,Vec,Vec,Vec);
extern int    PCApplyRichardson(PC,Vec,Vec,Vec,int);
extern int    PCApplyRichardsonExists(PC);
extern int    PCRegisterAll();
extern int    PCRegisterDestroy();
extern int    PCRegister(PCMethod,char *,int (*)(PC));
extern int    PCDestroy(PC);
extern int    PCSetFromOptions(PC);
extern int    PCGetMethodFromContext(PC,PCMethod*);
extern int    PCGetMethodName(PCMethod,char **);
extern int    PCGetFactoredMatrix(PC,Mat*);

/* Flags for PCSetOperators */
typedef enum {ALLMAT_DIFFERENT_NONZERO_PATTERN=0,MAT_SAME_NONZERO_PATTERN=1, 
              PMAT_SAME_NONZERO_PATTERN=2,ALLMAT_SAME_NONZERO_PATTERN=3}
              MatStructure;

extern int PCSetOperators(PC,Mat,Mat,MatStructure);
extern int PCBJacobiSetUseTrueLocal(PC);
extern int PCSetVector(PC,Vec);
extern int PCPrintHelp(PC);
extern int PCView(PC,Viewer);
extern int PCSetOptionsPrefix(PC,char*);

extern int PCSORSetSymmetric(PC, MatSORType);
extern int PCSORSetOmega(PC, double);
extern int PCEisenstatSetOmega(PC, double);
extern int PCSORSetIterations(PC, int);

extern int PCBJacobiSetTotalBlocks(PC, int, int*);
extern int PCBJacobiSetLocalBlocks(PC, int, int*);

extern int PCBSIterSetBlockSolve(PC);
extern int PCBSIterSetFromOptions(PC);
extern int PCBSIterSolve(PC,Vec,Vec,int*);

extern int PCShellSetApply(PC, int (*)(void*,Vec,Vec), void*);
extern int PCShellSetApplyRichardson(PC,int (*)(void*,Vec,Vec,Vec,int),void*);

extern int PCGetOperators(PC,Mat*,Mat*,MatStructure*);

extern int PCLUSetUseInPlace(PC);
extern int PCILUSetUseInPlace(PC);
extern int PCILUSetLevels(PC,int);
extern int PCEisenstatUseDiagonalScaling(PC);

extern int PCSPAISetParameters(PC,int,int,double);
extern int PCSPAIGetApproximateInverse(PC,Mat*);

#endif

