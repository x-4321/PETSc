

/* cannot have vcid because included in other files */

static int VecGetOwnershipRange_MPI(Vec v,int *low,int* high) 
{
  Vec_MPI *x = (Vec_MPI *) v->data;
  *low  = x->ownership[x->mytid];
  *high = x->ownership[x->mytid+1];
  return 0;
}

static int VecDestroy_MPI(PetscObject obj )
{
  Vec       v = (Vec ) obj;
  Vec_MPI *x = (Vec_MPI *) v->data;
#if defined(PETSC_LOG)
  PLogObjectState(obj,"Rows %d",x->N);
#endif  
  if (x->stash.array) FREE(x->stash.array);
  FREE(v->data);
  PLogObjectDestroy(v);
  PETSCHEADERDESTROY(v);
  return 0;
}

static int VecView_MPI( PetscObject obj, Viewer ptr )
{
  Vec         xin = (Vec) obj;
  Vec_MPI     *x = (Vec_MPI *) xin->data;
  int         i,mytid,numtid,ierr;
  MPI_Status  status;
  PetscObject vobj = (PetscObject) ptr;

  MPI_Comm_rank(x->comm,&mytid); 

  if (!ptr) { /* so that viewers may be used from debuggers */
    ptr = STDOUT_VIEWER; vobj = (PetscObject) ptr;
  }

  if (vobj->cookie == DRAW_COOKIE && vobj->type == NULLWINDOW) return 0;

  if (vobj->cookie == VIEWER_COOKIE) {
    FILE *fd = ViewerFileGetPointer_Private(ptr);
    if (vobj->type == FILE_VIEWER) {
      MPE_Seq_begin(x->comm,1);
      fprintf(fd,"Processor [%d] \n",mytid);
      for ( i=0; i<x->n; i++ ) {
#if defined(PETSC_COMPLEX)
        fprintf(fd,"%g + %g i\n",real(x->array[i]),imag(x->array[i]));
#else
        fprintf(fd,"%g \n",x->array[i]);
#endif
      }
      fflush(fd);
      MPE_Seq_end(x->comm,1);
    }
    else if (vobj->type == FILES_VIEWER) {
      int        len, work = x->n,n,j,numtids;
      Scalar     *values;
      /* determine maximum message to arrive */
      MPI_Reduce(&work,&len,1,MPI_INT,MPI_MAX,0,x->comm);
      MPI_Comm_size(x->comm,&numtids);

      if (!mytid) {
        values = (Scalar *) MALLOC( len*sizeof(Scalar) ); CHKPTR(values);
        fprintf(fd,"Processor [%d]\n",mytid);
        for ( i=0; i<x->n; i++ ) {
#if defined(PETSC_COMPLEX)
          fprintf(fd,"%g + %g i\n",real(x->array[i]),imag(x->array[i]));
#else
          fprintf(fd,"%g \n",x->array[i]);
#endif
        }
        /* receive and print messages */
        for ( j=1; j<numtids; j++ ) {
          MPI_Recv(values,len,MPI_SCALAR,j,47,x->comm,&status);
          MPI_Get_count(&status,MPI_SCALAR,&n);          
          fprintf(fd,"Processor [%d]\n",j);
          for ( i=0; i<n; i++ ) {
#if defined(PETSC_COMPLEX)
            fprintf(fd,"%g + %g i\n",real(values[i]),imag(values[i]));
#else
            fprintf(fd,"%g\n",values[i]);
#endif
          }          
        }
        FREE(values);
      }
      else {
        /* send values */
        MPI_Send(x->array,x->n,MPI_SCALAR,0,47,x->comm);
      }
    }
  }
  else if (vobj->cookie == DRAW_COOKIE) {
    DrawCtx win = (DrawCtx) ptr;
    int     start,end;
    double  coors[4],ymin,ymax,xmin,xmax,tmp;

    MPI_Comm_size(x->comm,&numtid); 

    xmin = 1.e20; xmax = -1.e20;
    for ( i=0; i<x->n; i++ ) {
#if defined(PETSC_COMPLEX)
      if (real(x->array[i]) < xmin) xmin = real(x->array[i]);
      else if (real(x->array[i]) > xmax) xmax = real(x->array[i]);
#else
      if (x->array[i] < xmin) xmin = x->array[i];
      else if (x->array[i] > xmax) xmax = x->array[i];
#endif
    }
    MPI_Reduce(&xmin,&ymin,1,MPI_DOUBLE,MPI_MIN,0,x->comm);
    MPI_Reduce(&xmax,&ymax,1,MPI_DOUBLE,MPI_MAX,0,x->comm);
    MPI_Comm_rank(x->comm,&mytid);
    if (!mytid) {
      DrawAxisCtx axis;
      DrawClear(win); DrawFlush(win);
      ierr = DrawAxisCreate(win,&axis); CHKERR(ierr);
      ierr = DrawAxisSetLimits(axis,0.0,(double) x->N,ymin,ymax); CHKERR(ierr);
      ierr = DrawAxis(axis); CHKERR(ierr);
      DrawAxisDestroy(axis);
      DrawGetCoordinates(win,coors,coors+1,coors+2,coors+3);
    }
    MPI_Bcast(coors,4,MPI_DOUBLE,0,x->comm);
    if (mytid) DrawSetCoordinates(win,coors[0],coors[1],coors[2],coors[3]);
    /* draw local part of vector */
    VecGetOwnershipRange(xin,&start,&end);
    if (mytid < numtid-1) { /*send value to right */
      MPI_Send(&x->array[x->n-1],1,MPI_DOUBLE,mytid+1,58,x->comm);
    }
    for ( i=1; i<x->n; i++ ) {
#if !defined(PETSC_COMPLEX)
      DrawLine(win,(double)(i-1+start),x->array[i-1],(double)(i+start),
                   x->array[i],DRAW_RED,DRAW_RED);
#else
      DrawLine(win,(double)(i-1+start),real(x->array[i-1]),(double)(i+start),
                   real(x->array[i]),DRAW_RED,DRAW_RED);
#endif
    }
    if (mytid) { /* receive value from right */
      MPI_Recv(&tmp,1,MPI_DOUBLE,mytid-1,58,x->comm,&status);
#if !defined(PETSC_COMPLEX)
      DrawLine(win,(double)start-1,tmp,(double)start,x->array[0],
                   DRAW_RED,DRAW_RED);
#else
      DrawLine(win,(double)start-1,tmp,(double)start,real(x->array[0]),
                    DRAW_RED,DRAW_RED);
#endif
    }
    DrawSyncFlush(win);
  }
  else if (vobj->cookie == LG_COOKIE){
    DrawLGCtx lg = (DrawLGCtx) ptr;
    DrawCtx   win;
    double    *xx,*yy;
    int       N = x->N,*lens;
    MPI_Comm_rank(x->comm,&mytid);
    MPI_Comm_size(x->comm,&numtid);
    if (!mytid) {
      DrawLGReset(lg);
      xx = (double *) MALLOC( 2*N*sizeof(double) ); CHKPTR(xx);
      for ( i=0; i<N; i++ ) {xx[i] = (double) i;}
      yy = xx + N;
      lens = (int *) MALLOC(numtid*sizeof(int)); CHKPTR(lens);
      for (i=0; i<numtid; i++ ) {
        lens[i] = x->ownership[i+1] - x->ownership[i];
      }
      /* The next line is wrong for complex, one should stride out the 
         real part of x->array and Gatherv that */
      MPI_Gatherv(x->array,x->n,MPI_DOUBLE,yy,lens,x->ownership,MPI_DOUBLE,
                  0,x->comm);
      FREE(lens);
      DrawLGAddPoints(lg,N,&xx,&yy);
      FREE(xx);
      DrawLG(lg);
    }
    else {
      /* The next line is wrong for complex, one should stride out the 
         real part of x->array and Gatherv that */
      MPI_Gatherv(x->array,x->n,MPI_DOUBLE,0,0,0,MPI_DOUBLE,0,x->comm);
    }
    DrawLGGetDrawCtx(lg,&win);
    DrawSyncFlush(win);
  }
  return 0;
}

static int VecGetSize_MPI(Vec xin,int *N)
{
  Vec_MPI  *x = (Vec_MPI *)xin->data;
  *N = x->N;
  return 0;
}
/*
      Uses a slow search to determine if item is already cached. 
   Could keep cache list sorted at all times.
*/
static int VecSetValues_MPI(Vec xin, int ni, int *ix, Scalar* y,
                                   InsertMode addv )
{
  Vec_MPI  *x = (Vec_MPI *)xin->data;
  int        mytid = x->mytid, *owners = x->ownership, start = owners[mytid];
  int        end = owners[mytid+1], i, j, alreadycached;
  Scalar     *xx = x->array;

#if defined(PETSC_DEBUG)
  if (x->insertmode == InsertValues && addv == AddValues) {
    SETERR(1,"You have already inserted vector values, you cannot now add");
  }
  else if (x->insertmode == AddValues && addv == InsertValues) {
    SETERR(1,"You have already added vector values, you cannot now insert");
  }
#endif
  x->insertmode = addv;

  for ( i=0; i<ni; i++ ) {
    if ( ix[i] >= start && ix[i] < end) {
      if (addv == InsertValues) xx[ix[i]-start] = y[i];
      else                      xx[ix[i]-start] += y[i];
    }
    else {
#if defined(PETSC_DEBUG)
      if (ix[i] < 0 || ix[i] > x->N) SETERR(1,"Index out of range");
#endif
      /* check if this index has already been cached */
      alreadycached = 0;
      for ( j=0; j<x->stash.n; j++ ) {
        if (x->stash.idx[j] == ix[i]) {
          if (addv == InsertValues) x->stash.array[j] = y[i];
          else                      x->stash.array[j] += y[i];
          alreadycached = 1; 
          break;
        }
      }
      if (!alreadycached) {
        if (x->stash.n == x->stash.nmax) {/* cache is full */
          int    *idx, nmax = x->stash.nmax;
          Scalar *array;
          array = (Scalar *) MALLOC( (nmax+10)*sizeof(Scalar) + 
                                     (nmax+10)*sizeof(int) ); CHKPTR(array);
          idx = (int *) (array + nmax + 10);
          MEMCPY(array,x->stash.array,nmax*sizeof(Scalar));
          MEMCPY(idx,x->stash.idx,nmax*sizeof(int));
          if (x->stash.array) FREE(x->stash.array);
          x->stash.array = array; x->stash.idx = idx;
          x->stash.nmax += 10;
        }
        x->stash.array[x->stash.n] = y[i];
        x->stash.idx[x->stash.n++] = ix[i];
      }
    }
  }
  return 0;
}

/*
   Since nsends or nreceives may be zero we add 1 in certain mallocs
to make sure we never malloc an empty one.      
*/
static int VecBeginAssembly_MPI(Vec xin)
{
  Vec_MPI   *x = (Vec_MPI *)xin->data;
  int         mytid = x->mytid, *owners = x->ownership, numtids = x->numtids;
  int         *nprocs,i,j,idx,*procs,nsends,nreceives,nmax,*work;
  int         *owner,*starts,count,tag = 22;
  InsertMode  addv;
  Scalar      *rvalues,*svalues;
  MPI_Comm    comm = x->comm;
  MPI_Request *send_waits,*recv_waits;

  /* make sure all processors are either in INSERTMODE or ADDMODE */
  MPI_Allreduce((void *) &x->insertmode,(void *) &addv,1,MPI_INT,
                MPI_BOR,comm);
  if (addv == (AddValues|InsertValues)) {
    SETERR(1,"Some processors have inserted while others have added");
  }
  x->insertmode = addv; /* in case this processor had no cache */

  /*  first count number of contributors to each processor */
  nprocs = (int *) MALLOC( 2*numtids*sizeof(int) ); CHKPTR(nprocs);
  MEMSET(nprocs,0,2*numtids*sizeof(int)); procs = nprocs + numtids;
  owner = (int *) MALLOC( (x->stash.n+1)*sizeof(int) ); CHKPTR(owner);
  for ( i=0; i<x->stash.n; i++ ) {
    idx = x->stash.idx[i];
    for ( j=0; j<numtids; j++ ) {
      if (idx >= owners[j] && idx < owners[j+1]) {
        nprocs[j]++; procs[j] = 1; owner[i] = j; break;
      }
    }
  }
  nsends = 0;  for ( i=0; i<numtids; i++ ) { nsends += procs[i];} 

  /* inform other processors of number of messages and max length*/
  work = (int *) MALLOC( numtids*sizeof(int) ); CHKPTR(work);
  MPI_Allreduce((void *) procs,(void *) work,numtids,MPI_INT,MPI_SUM,comm);
  nreceives = work[mytid]; 
  MPI_Allreduce((void *) nprocs,(void *) work,numtids,MPI_INT,MPI_MAX,comm);
  nmax = work[mytid];
  FREE(work);

  /* post receives: 
       1) each message will consist of ordered pairs 
     (global index,value) we store the global index as a double 
     to simply the message passing. 
       2) since we don't know how long each individual message is we 
     allocate the largest needed buffer for each receive. Potentially 
     this is a lot of wasted space.
       This could be done better.
  */
  rvalues = (Scalar *) MALLOC(2*(nreceives+1)*(nmax+1)*sizeof(Scalar));
  CHKPTR(rvalues);
  recv_waits = (MPI_Request *) MALLOC((nreceives+1)*sizeof(MPI_Request));
  CHKPTR(recv_waits);
  for ( i=0; i<nreceives; i++ ) {
    MPI_Irecv((void *)(rvalues+2*nmax*i),2*nmax,MPI_SCALAR,MPI_ANY_SOURCE,tag,
              comm,recv_waits+i);
  }

  /* do sends:
      1) starts[i] gives the starting index in svalues for stuff going to 
         the ith processor
  */
  svalues = (Scalar *) MALLOC( 2*(x->stash.n+1)*sizeof(Scalar) );
  CHKPTR(svalues);
  send_waits = (MPI_Request *) MALLOC( (nsends+1)*sizeof(MPI_Request));
  CHKPTR(send_waits);
  starts = (int *) MALLOC( numtids*sizeof(int) ); CHKPTR(starts);
  starts[0] = 0; 
  for ( i=1; i<numtids; i++ ) { starts[i] = starts[i-1] + nprocs[i-1];} 
  for ( i=0; i<x->stash.n; i++ ) {
    svalues[2*starts[owner[i]]]       = (Scalar)  x->stash.idx[i];
    svalues[2*(starts[owner[i]]++)+1] =  x->stash.array[i];
  }
  FREE(owner);
  starts[0] = 0;
  for ( i=1; i<numtids; i++ ) { starts[i] = starts[i-1] + nprocs[i-1];} 
  count = 0;
  for ( i=0; i<numtids; i++ ) {
    if (procs[i]) {
      MPI_Isend((void*)(svalues+2*starts[i]),2*nprocs[i],MPI_SCALAR,i,tag,
                comm,send_waits+count++);
    }
  }
  FREE(starts); FREE(nprocs);

  /* Free cache space */
  x->stash.nmax = x->stash.n = 0;
  if (x->stash.array){ FREE(x->stash.array); x->stash.array = 0;}

  x->svalues    = svalues;       x->rvalues = rvalues;
  x->nsends     = nsends;         x->nrecvs = nreceives;
  x->send_waits = send_waits; x->recv_waits = recv_waits;
  x->rmax       = nmax;
  
  return 0;
}

static int VecEndAssembly_MPI(Vec vec)
{
  Vec_MPI   *x = (Vec_MPI *)vec->data;
  MPI_Status  *send_status,recv_status;
  int         imdex,base,nrecvs = x->nrecvs, count = nrecvs, i, n;
  Scalar      *values;

  base = x->ownership[x->mytid];

  /*  wait on receives */
  while (count) {
    MPI_Waitany(nrecvs,x->recv_waits,&imdex,&recv_status);
    /* unpack receives into our local space */
    values = x->rvalues + 2*imdex*x->rmax;
    MPI_Get_count(&recv_status,MPI_SCALAR,&n);
    n = n/2;
    if (x->insertmode == AddValues) {
      for ( i=0; i<n; i++ ) {
        x->array[((int) PETSCREAL(values[2*i])) - base] += values[2*i+1];
      }
    }
    else if (x->insertmode == InsertValues) {
      for ( i=0; i<n; i++ ) {
        x->array[((int) PETSCREAL(values[2*i])) - base] = values[2*i+1];
      }
    }
    else {
        SETERR(1,"Insert mode is not set correct; corrupt vector");
    }
    count--;
  }
  FREE(x->recv_waits); FREE(x->rvalues);
 
  /* wait on sends */
  if (x->nsends) {
    send_status = (MPI_Status *) MALLOC( x->nsends*sizeof(MPI_Status) );
    CHKPTR(send_status);
    MPI_Waitall(x->nsends,x->send_waits,send_status);
    FREE(send_status);
  }
  FREE(x->send_waits); FREE(x->svalues);

  x->insertmode = NotSetValues;
  return 0;
}

