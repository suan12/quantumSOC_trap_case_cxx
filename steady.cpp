#include <petscksp.h>
#include "steady.h"
#undef __FUNCT__
#define __FUNCT__ "steady"

void cMasterMatrix::initialize(){
	MPI_Comm_size(PETSC_COMM_WORLD,&size);
	MPI_Comm_rank(PETSC_COMM_WORLD,&rank);
//	cout << rank << '\t' << size << endl;
	for (int ig = 0; ig < size; ++ig) {
	    if (ig ==rank){
	    	char dummyname[100];
	    	double dummyvalue;
	    	int intdummyvalue;
	        FILE *input;
	        input = fopen("input.txt","r");
	        assert(input != NULL);
	        if (ig == 0)  cout << "Starting to read in parameters from file input.txt" << endl;
	        fscanf(input,"%s %d", dummyname, &intdummyvalue);
	        N = intdummyvalue;    if (ig == 0) cout << dummyname << "=" << N << endl;
	        fscanf(input,"%s %d", dummyname, &intdummyvalue);
	        Q = intdummyvalue;    if (ig == 0) cout << dummyname << "=" << Q << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        omega = dummyvalue;    if (ig == 0) cout << dummyname << "=" << omega << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        qr = dummyvalue;    if (ig == 0) cout << dummyname << "=" << qr << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        Omega = dummyvalue;    if (ig == 0) cout << dummyname << "=" << Omega << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        delta = dummyvalue;    if (ig == 0) cout << dummyname << "=" << delta << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        varepsilon = dummyvalue;    if (ig == 0) cout << dummyname << "=" << varepsilon << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        delta_c = dummyvalue;    if (ig == 0) cout << dummyname << "=" << delta_c << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        kappa = dummyvalue;    if (ig == 0) cout << dummyname << "=" << kappa << endl;
	        fscanf(input,"%s %lf", dummyname, &dummyvalue);
	        tol = dummyvalue;    if (ig == 0) cout << dummyname << "=" << tol << endl;
	        fclose(input);
	    }
	}
//  qr = 1;
//  Omega = 1;
//  delta = 4;
//  varepsilon = 1;
//  delta_c = 1;
//  kappa = 1;
//  N=2;
//  Q=1;
	//  tol=1.e-11;
	DIM   = 4*(N+1)*(N+1)*(Q+1)*(Q+1);
	tDIM1 = (N+1)*(N+1)*(Q+1)*(Q+1);
	tDIM2 = (N+1)*(Q+1)*(Q+1);
	tDIM3 = (Q+1)*(Q+1);
	tDIM4 = (Q+1);
  one =1.0;neg_one=-1.0;
}

void cMasterMatrix::block(int irow, int&r, int&m, int&n, int&p, int&q){
  /*
    Decompose a given global row/column index into block and sub-block indices.
    Based on the formulation we have from Master equation, we have the knowledge of dense construction,
    which has been tested in MATLAB format. Scaling up, we need to deploy sparse format and thus, a map
    that links the correspondance between the two.
  */
  int k;
  r = floor(irow/tDIM1);
  k = irow-r*tDIM1;
  m = floor(k/tDIM2);
  n = floor((k-m*tDIM2)/tDIM3);
  p = floor((k-m*tDIM2-n*tDIM3)/tDIM4);
  q = k-(m*tDIM2+n*tDIM3+p*tDIM4);
}

PetscErrorCode cMasterMatrix::construction(){
  /*
    Create matrix.  When using MatCreate(), the matrix format can
    be specified at runtime.
    
    Performance tuning note:  For problems of substantial size,
    preallocation of matrix memory is crucial for attaining good
    performance. See the matrix chapter of the users manual for details.

  */
  ierr = MatCreate(PETSC_COMM_WORLD,&G);CHKERRQ(ierr);
  ierr = MatSetType(G,MATMPIAIJ);CHKERRQ(ierr);
  ierr = MatSetSizes(G,PETSC_DECIDE,PETSC_DECIDE,DIM,DIM);CHKERRQ(ierr);
  // The following set up is not needed for pre-allocated matrix. --> debugging purpose only.
  //	  ierr = MatSetFromOptions(G);CHKERRQ(ierr);
  //	  ierr = MatSetUp(G);CHKERRQ(ierr);

  //	ierr = MatMPIAIJSetPreallocation(G,__MAXNOZEROS__,NULL,__MAXNOZEROS__,NULL);CHKERRQ(ierr);

  ierr = MatMPIAIJSetPreallocation(G,10,NULL,10,NULL);CHKERRQ(ierr);

  ierr = MatGetOwnershipRange(G,&rstart,&rend);CHKERRQ(ierr);
  ierr = MatGetLocalSize(G,&nlocal, NULL);CHKERRQ(ierr);

  ierr = assemblance();CHKERRQ(ierr);

  ierr = MatAssemblyBegin(G,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(G,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  // Modify matrix and impose Trace of \rho = 1 as a constraint explicitly.
//  PetscInt val0; val0 = 0;
//  ierr = MatZeroRows(G, 1, &val0, 0.0, 0, 0);CHKERRQ(ierr); // This has to be done AFTER matrix final assembly by petsc
  ierr = MatSetOption(G, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_FALSE);CHKERRQ(ierr);
  PetscInt* col0;
  PetscScalar *value0;
  ierr = PetscMalloc1(2*(N+1)*(Q+1),&col0);CHKERRQ(ierr);
  ierr = PetscMalloc1(2*(N+1)*(Q+1),&value0);CHKERRQ(ierr);
  if (rstart == 0) {
    int nonzeros = 0;
    for (m=0;m<=N;m++){
      for (p=0;p<=Q;p++){
	col0[nonzeros] = m*tDIM2+m*tDIM3+p*tDIM4+p; // rho_up_up
	value0[nonzeros] = 1.0;
	nonzeros ++;
	col0[nonzeros] = 3*tDIM1+col0[nonzeros-1]; // rho_dn_dn
	value0[nonzeros] = 1.0;
	nonzeros ++;
      }
    }
//    cout << nonzeros << endl;

    ierr = MatSetValues(G,1,&rstart,nonzeros,col0,value0,INSERT_VALUES);CHKERRQ(ierr);
  }
  ierr = PetscFree(col0);ierr = PetscFree(value0);
//  /* Re-Assemble the matrix */
  ierr = MatAssemblyBegin(G,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(G,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatSetOption(G, MAT_NEW_NONZERO_ALLOCATION_ERR, PETSC_TRUE);CHKERRQ(ierr);
  /*
      Create vectors.  Note that we form 1 vector from scratch and
      then duplicate as needed. For this simple case let PETSc decide how
      many elements of the vector are stored on each processor.
    */
    ierr = VecCreate(PETSC_COMM_WORLD,&x);CHKERRQ(ierr);
    ierr = VecSetSizes(x,nlocal,DIM);CHKERRQ(ierr);
    ierr = VecSetFromOptions(x);CHKERRQ(ierr);
    ierr = VecDuplicate(x,&b);CHKERRQ(ierr);
    ierr = VecDuplicate(x,&u);CHKERRQ(ierr);
  return ierr;
}


PetscErrorCode cMasterMatrix::viewMatrix(){
// Runtime option using database keys:  -mat_view draw -draw_pause -1

//	ierr = PetscViewerSetFormat(PETSC_VIEWER_STDOUT_WORLD,	PETSC_VIEWER_ASCII_DENSE  );CHKERRQ(ierr);
//  ierr = PetscViewerSetFormat(PETSC_VIEWER_STDOUT_WORLD,	PETSC_VIEWER_ASCII_MATLAB  );CHKERRQ(ierr);
//  ierr = MatView(G,	PETSC_VIEWER_STDOUT_WORLD );CHKERRQ(ierr);

//Vec tmpu;
//ierr = VecDuplicate(x,&tmpu);CHKERRQ(ierr);
//ierr = MatMult(G,b,tmpu);CHKERRQ(ierr);
//ierr = VecView(tmpu,	PETSC_VIEWER_STDOUT_WORLD );CHKERRQ(ierr);

//  PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"",300,0,300,300,&viewer);
//	  ierr = MatView(G,	viewer );CHKERRQ(ierr);
//	  ierr = VecView(b,	PETSC_VIEWER_STDOUT_WORLD );CHKERRQ(ierr);
//    ierr = VecView(x,	PETSC_VIEWER_STDOUT_WORLD );CHKERRQ(ierr);
	return ierr;
}

//void cMasterMatrix::MatInsert(PetscScalar _val_, int &nonzeros, PetscInt* col, PetscScalar* value,
//					int ct, int mt, int nt, int pt, int qt){
////  if (PetscAbsScalar(_val_) != 0 ) {
//    col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
////    cout << nonzeros << endl;
////  }
//
//}

PetscErrorCode cMasterMatrix::assemblance(){
  /*
    Assemble matrix.
    
    The linear system is distributed across the processors by
    chunks of contiguous rows, which correspond to contiguous
    sections of the mesh on which the problem is discretized.
    For matrix assembly, each processor contributes entries for
    the part that it owns locally.
  */
  int nonzeros; // TODO: check if nonzeros < __MAXNOZEROS__ is true.
  int ct, mt, nt, pt, qt;
  int kt;
  PetscScalar _val_;
  for (ROW=rstart; ROW<rend; ROW++) {
	  nonzeros = 0;
	  block(ROW,r,m,n,p,q);
    switch (r) {
    case 0:
    	if (ROW != 0) { // Impose Tr[\rho]=1 condition at the first row later.
    	// MUU block
    	ct = r; mt = m; nt = n; pt = p; qt = q;
	_val_ = ((p+0.5)*omega+delta)/PETSC_i-((q+0.5)*omega+delta)/PETSC_i+PETSC_i*delta_c*double(m-n)-kappa*double(m+n);
	col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	ct = r; mt = m; nt = n; pt = p+1; qt = q;
    	if (pt <= Q) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(p+1);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p-1; qt = q;
    	if (pt >= 0) {
	  _val_ =qr*sqrt(omega)/sqrt(2)*sqrt(p);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q+1;
    	if (qt <= Q) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(q+1);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q-1;
    	if (qt >= 0) {
	  _val_ = qr*sqrt(omega)/sqrt(2)*sqrt(q);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n+1; pt = p; qt = q;
    	if (mt <= N && nt <= N) {
	  _val_ = kappa*2.0*sqrt(m+1)*sqrt(n+1);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n; pt = p; qt = q;
    	if (mt <= N) {
	  _val_ = -varepsilon*sqrt(m+1);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m-1; nt = n; pt = p; qt = q;
    	if (mt >= 0) {
	  _val_ = varepsilon*sqrt(m);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n+1; pt = p; qt = q;
    	if (nt <= N) {
	  _val_ = -varepsilon*sqrt(n+1);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n-1; pt = p; qt = q;
    	if (nt >= 0) {
	  _val_ = varepsilon*sqrt(n);
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S1 block
    	ct = 1;mt = m; nt = n+1; pt = p; qt = q;
    	if (nt <= N) {
	  _val_ = -Omega/2.0*sqrt(n+1)/PETSC_i;
	  // TODO: potential bugs for MatView with pure imaginary number display.
	  //    		cout << ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt << '\t' << PetscAbsScalar(_val_) << endl;
	  col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S2 block
    	ct = 2;mt = m+1; nt = n; pt = p; qt = q;
    	if (mt <= N) {
    		_val_ = Omega/2.0*sqrt(m+1)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
        if (nonzeros > __MAXNOZEROS__){
        	cerr << "nonzeros on a row " <<  nonzeros << " is larger than the pre-allocated range of"
        	<<  __MAXNOZEROS__ <<" const arrays. Try increasing the max number in steady.h" << endl;exit(1);
        }
        ierr   = MatSetValues(G,1,&ROW,nonzeros,col,value,INSERT_VALUES);CHKERRQ(ierr);
    	}
    	break;
    case 1:
    	// MUD block
    	ct = r; mt = m; nt = n; pt = p; qt = q;
			_val_ = ((p+0.5)*omega+delta)/PETSC_i-((q+0.5)*omega-delta)/PETSC_i+PETSC_i*delta_c*double(m-n)-kappa*double(m+n);
			col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	ct = r; mt = m; nt = n; pt = p+1; qt = q;
    	if (pt <= Q) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(p+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p-1; qt = q;
    	if (pt >= 0) {
	  _val_ = qr*sqrt(omega)/sqrt(2)*sqrt(p);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q+1;
    	if (qt <= Q) {
	  _val_ = qr*sqrt(omega)/sqrt(2)*sqrt(q+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q-1;
    	if (qt >= 0) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(q);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n+1; pt = p; qt = q;
    	if (mt <= N && nt <= N) {
    		_val_ = kappa*2.0*sqrt(m+1)*sqrt(n+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n; pt = p; qt = q;
    	if (mt <= N) {
    		_val_ = -varepsilon*sqrt(m+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m-1; nt = n; pt = p; qt = q;
    	if (mt >= 0) {
    		_val_ = varepsilon*sqrt(m);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n+1; pt = p; qt = q;
    	if (nt <= N) {
    		_val_ = -varepsilon*sqrt(n+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n-1; pt = p; qt = q;
    	if (nt >= 0) {
    		_val_ = varepsilon*sqrt(n);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S3 block
    	ct = 0;mt = m; nt = n-1; pt = p; qt = q;
    	if (nt >= 0) {
    		_val_ = -Omega/2.0*sqrt(n)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S4 block
    	ct = 3;mt = m+1; nt = n; pt = p; qt = q;
    	if (mt <= N) {
    		_val_ = Omega/2.0*sqrt(m+1)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
        if (nonzeros > __MAXNOZEROS__){
        	cerr << "nonzeros on a row " <<  nonzeros << " is larger than the pre-allocated range of"
        	<<  __MAXNOZEROS__ <<" const arrays. Try increasing the max number in steady.h" << endl;exit(1);
        }
        ierr   = MatSetValues(G,1,&ROW,nonzeros,col,value,INSERT_VALUES);CHKERRQ(ierr);
    	break;
    case 2:
    	// MDU block
    	ct = r; mt = m; nt = n; pt = p; qt = q;
			_val_ = ((p+0.5)*omega-delta)/PETSC_i-((q+0.5)*omega+delta)/PETSC_i+PETSC_i*delta_c*double(m-n)-kappa*double(m+n);
			col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	ct = r; mt = m; nt = n; pt = p+1; qt = q;
    	if (pt <= Q) {
	  _val_ = qr*sqrt(omega)/sqrt(2)*sqrt(p+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p-1; qt = q;
    	if (pt >= 0) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(p);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q+1;
    	if (qt <= Q) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(q+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q-1;
    	if (qt >= 0) {
	  _val_ =  qr*sqrt(omega)/sqrt(2)*sqrt(q);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n+1; pt = p; qt = q;
    	if (mt <= N && nt <= N) {
    		_val_ = kappa*2.0*sqrt(m+1)*sqrt(n+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n; pt = p; qt = q;
    	if (mt <= N) {
    		_val_ = -varepsilon*sqrt(m+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m-1; nt = n; pt = p; qt = q;
    	if (mt >= 0) {
    		_val_ = varepsilon*sqrt(m);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n+1; pt = p; qt = q;
    	if (nt <= N) {
    		_val_ = -varepsilon*sqrt(n+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n-1; pt = p; qt = q;
    	if (nt >= 0) {
    		_val_ = varepsilon*sqrt(n);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S5 block
    	ct = 0;mt = m-1; nt = n; pt = p; qt = q;
    	if (mt >= 0) {
    		_val_ = Omega/2.0*sqrt(m)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S6 block
    	ct = 3;mt = m; nt = n+1; pt = p; qt = q;
    	if (nt <= N) {
    		_val_ = -Omega/2.0*sqrt(n+1)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
        if (nonzeros > __MAXNOZEROS__){
        	cerr << "nonzeros on a row " <<  nonzeros << " is larger than the pre-allocated range of"
        	<<  __MAXNOZEROS__ <<" const arrays. Try increasing the max number in steady.h" << endl;exit(1);
        }
        ierr   = MatSetValues(G,1,&ROW,nonzeros,col,value,INSERT_VALUES);CHKERRQ(ierr);
        break;
    case 3:
    	// MDD block
    	ct = r; mt = m; nt = n; pt = p; qt = q;
			_val_ = ((p+0.5)*omega-delta)/PETSC_i-((q+0.5)*omega-delta)/PETSC_i+PETSC_i*delta_c*double(m-n)-kappa*double(m+n);
			col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	ct = r; mt = m; nt = n; pt = p+1; qt = q;
    	if (pt <= Q) {
	  _val_ = qr*sqrt(omega)/sqrt(2)*sqrt(p+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p-1; qt = q;
    	if (pt >= 0) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(p);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q+1;
    	if (qt <= Q) {
	  _val_ = qr*sqrt(omega)/sqrt(2)*sqrt(q+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n; pt = p; qt = q-1;
    	if (qt >= 0) {
	  _val_ = -qr*sqrt(omega)/sqrt(2)*sqrt(q);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n+1; pt = p; qt = q;
    	if (mt <= N && nt <= N) {
    		_val_ = kappa*2.0*sqrt(m+1)*sqrt(n+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m+1; nt = n; pt = p; qt = q;
    	if (mt <= N) {
    		_val_ = -varepsilon*sqrt(m+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m-1; nt = n; pt = p; qt = q;
    	if (mt >= 0) {
    		_val_ = varepsilon*sqrt(m);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n+1; pt = p; qt = q;
    	if (nt <= N) {
    		_val_ = -varepsilon*sqrt(n+1);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	ct = r; mt = m; nt = n-1; pt = p; qt = q;
    	if (nt >= 0) {
    		_val_ = varepsilon*sqrt(n);
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S7 block
    	ct = 1;mt = m-1; nt = n; pt = p; qt = q;
    	if (mt >= 0) {
    		_val_ = Omega/2.0*sqrt(m)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
    	// S8 block
    	ct = 2;mt = m; nt = n-1; pt = p; qt = q;
    	if (nt >= 0) {
    		_val_ = -Omega/2.0*sqrt(n)/PETSC_i;
    		col[nonzeros] = ct*tDIM1+mt*tDIM2+nt*tDIM3+pt*tDIM4+qt;value[nonzeros] = _val_;nonzeros ++;
    	}
        if (nonzeros > __MAXNOZEROS__){
        	cerr << "nonzeros on a row " <<  nonzeros << " is larger than the pre-allocated range of"
        	<<  __MAXNOZEROS__ <<" const arrays. Try increasing the max number in steady.h" << endl;exit(1);
        }
        ierr   = MatSetValues(G,1,&ROW,nonzeros,col,value,INSERT_VALUES);CHKERRQ(ierr);
        break;
    default:
    	cerr << "Sub-block row index" << r << " is out of range from 0 to 3. Stopping now..." << endl;
    	exit(1);
    	break;
    }
  }
  return ierr;
}


PetscErrorCode cMasterMatrix::seek_steady_state(){
  /*
    Set exact solution; then compute right-hand-side vector.
  */
//  ierr = VecSet(u,one);CHKERRQ(ierr);
//  ierr = MatMult(G,u,b);CHKERRQ(ierr);
  for (ROW=rstart;ROW<rend;ROW++){
    if (ROW==0) {
      val=1.0;
      ierr = VecSetValues(b,1,&ROW,&val, INSERT_VALUES);
    }
    else {
      val=0.0;
      ierr = VecSetValues(b,1,&ROW,&val, INSERT_VALUES);
    }
  }

  ierr =  VecAssemblyBegin(b); CHKERRQ(ierr);
  ierr =  VecAssemblyEnd(b); CHKERRQ(ierr);
//  ierr = PetscViewerSetFormat(PETSC_VIEWER_STDOUT_WORLD,	PETSC_VIEWER_ASCII_MATLAB  );CHKERRQ(ierr);
//  ierr = VecView(b,	PETSC_VIEWER_STDOUT_WORLD );CHKERRQ(ierr);
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                 Create the linear solver and set various options
      - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
   /*
      Create linear solver context
   */
   ierr = KSPCreate(PETSC_COMM_WORLD,&ksp);CHKERRQ(ierr);

   /*
      Set operators. Here the matrix that defines the linear system
      also serves as the preconditioning matrix.
   */
   ierr = KSPSetOperators(ksp,G ,G);CHKERRQ(ierr);
   /*
       Set linear solver defaults for this problem (optional).
       - By extracting the KSP and PC contexts from the KSP context,
         we can then directly call any KSP and PC routines to set
         various options.
       - The following four statements are optional; all of these
         parameters could alternatively be specified at runtime via
         KSPSetFromOptions();
    */
//    ierr = KSPGetPC(ksp,&pc);CHKERRQ(ierr);
//    ierr = PCSetType(pc,PCJACOBI);CHKERRQ(ierr);
    ierr = KSPSetTolerances(ksp,tol,PETSC_DEFAULT,PETSC_DEFAULT,PETSC_DEFAULT);CHKERRQ(ierr);

    /*
      Set runtime options, e.g.,
          -ksp_type <type> -pc_type <type> -ksp_monitor -ksp_rtol <rtol>
      These options will override those specified above as long as
      KSPSetFromOptions() is called _after_ any other customization
      routines.
    */
    ierr = KSPSetFromOptions(ksp);CHKERRQ(ierr);

    /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                        Solve the linear system
       - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
    /*
       Solve linear system
    */
//    KSPSetInitialGuessNonzero(ksp,PETSC_TRUE);
    ierr = KSPSolve(ksp,b,x);CHKERRQ(ierr);

    /*
       View solver info; we could instead use the option -ksp_view to
       print this info to the screen at the conclusion of KSPSolve().
    */
    ierr = KSPView(ksp,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
                         Check solution and clean up
        - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
     /*
        Check the error
     */
//    ierr = PetscViewerSetFormat(PETSC_VIEWER_STDOUT_WORLD,PETSC_VIEWER_ASCII_MATLAB);
//    ierr = VecView(x,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
//     ierr = VecAXPY(x,neg_one,u);CHKERRQ(ierr);
//     ierr = VecNorm(x,NORM_2,&norm);CHKERRQ(ierr);
//     ierr = KSPGetIterationNumber(ksp,&its);CHKERRQ(ierr);
//       if (norm > tol) {
//       ierr = PetscPrintf(PETSC_COMM_WORLD,"Norm of error %g, Iterations %D\n",(double)norm,its);CHKERRQ(ierr);
//       }

    return ierr;
}

PetscErrorCode cMasterMatrix::destruction(){
  /*
    Free work space.  All PETSc objects should be destroyed when they
    are no longer needed.
  */
  
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&b);CHKERRQ(ierr); ierr = MatDestroy(&G);CHKERRQ(ierr);
  ierr = KSPDestroy(&ksp);CHKERRQ(ierr);
  return ierr;
}
