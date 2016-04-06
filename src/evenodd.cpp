/*! \file evenodd.cpp
 *  \brief Contains functions to calculate even-odd designs
 *
 */

#include <printfheader.h>

#include "arraytools.h"
#include "arrayproperties.h"
#include "strength.h"

#ifdef DOOPENMP
#include "omp.h"
#endif


#include "evenodd.h"

#ifndef myprintf
#define myprintf printf
#endif

#ifdef MAINMEX
#define MATLABUPDATE
#else
#ifdef MATLAB_MEX
#define MATLABUPDATE   mexEvalString ("drawnow" );
#else
#define MATLABUPDATE
#endif
#endif

/// helper function, OpenMP variation of depth_extend
void depth_extend_omp ( const arraylist_t &alist,  depth_extend_t &dextend, depth_extend_sub_t &dextendsub, int extcol, int verbose=1, int nomp = 0 );




/// helper function
std::string depth_extend_logstring ( int n )
{
	std::string sp="";
	for ( int i=0; i<n; i++ )
		sp+="   ";
	sp+= "|";
	return sp;
}
/// initialize the new list of extension columns
arraylist_t depth_extend_sub_t::initialize ( const arraylist_t& alist, const arraydata_t &adf, const OAextend &oaextend )
{
	myassert ( alist.size() == lmctype.size() , "depth_extend_t: update" );

	int ncolsx=0;

	valididx.clear();

	if ( alist.size() >0 ) {
		ncolsx=alist[0].n_columns;
	} else {
		arraylist_t v;
		return v;
	}
	arraydata_t ad ( &adf, ncolsx );

	if ( verbose>=2 )
		printfd ( "initialize: %d \n", alist.size() );

	#pragma omp parallel for schedule(dynamic,1)	// FIXME: implement this
	for ( int k=0; k<(int)alist.size(); ++k ) {
		LMCreduction_t reduction ( &ad );
		// needed to make the code thread-safe
		reduction.initStatic();

		reduction.reset();
		reduction.setArray ( alist[k] );
		lmc_t rx = LMC_EQUAL;
		reduction.updateSDpointer ( alist[k] );
		/*
		if ( 0 ) { // FIXME: enable this?
			reduction.updateSDpointer ( alist[k] );
			int col=alist[k].n_columns-1;
			rx = LMC_check_col_rowsymm ( alist[k].array+alist[k].n_rows* ( alist[k].n_columns-1 ), &ad,  *reduction.sd.get(),col, 0 );
			printf ( "\n%s: rx %d\n", __FUNCTION__, rx );

			printf ( "col: " );
			print_perm ( alist[k].array+alist[k].n_rows* ( alist[k].n_columns-1 ), ad.N );
		} */

		lmc_t lmc =  LMCcheck ( alist[k].array, ad, oaextend, reduction );
		int lc=reduction.lastcol;
//#pragma omp critical
		{
			this->lmctype[k]=lmc;
			this->lastcol[k]=lc;
		}
		if ( verbose>=2 ) {
			printf ( "   depth_extend_sub_t.initialize: initialize: array %d lmc %d\n", k, ( int ) lmc );
			//  printf("   -> lastcol %d md5: %s\n", this->lastcol[k], alist[k].md5().c_str() );
			//oaextend.info(); reduction->show();

		}

		if ( k>25 && 0 ) {
			printf ( "file %s: line %d: exit\n", __FILE__, __LINE__ );
			exit ( 0 );
		}
		reduction.releaseStatic();
	}


	for ( size_t k=0; k<alist.size(); ++k ) {
		if ( verbose>=1 ) {
			printf ( "  depth_extend_sub_t.initialize: array %ld: lmc %d, lastcol %d, ncolumns %d\n", k, ( int ) this->lmctype[k], this->lastcol[k], ncolsx );
		}
		bool b1= ( this->lastcol[k]>=ncolsx || this->lastcol[k]==-1 );
		bool b2 = lmctype[k]>=LMC_EQUAL;
		if ( b1!=b2 ) {
			printf ( "oadevelop:initialize: huh? b1 %d b2 %d, lmctype[k] %d, lastcol[k] %d, col %d\n", b1, b2, lmctype[k], lastcol[k], ncolsx );
			oaextend.info();
			//writearrayfile("/home/eendebakpt/tmp/tmp.oa", alist[k]);
			//exit(0);

		}

		if ( this->lastcol[k]>=ncolsx-1 || this->lastcol[k]==-1 ) {
			valididx.push_back ( k );
		} else {
			if ( lmctype[k]>=LMC_EQUAL ) {
				//printf("oadevelop.h: initialize: huh?\n");
				//exit(0);
			}
		}
	}

	if ( verbose )
		printf ( "depth_extend_sub_t: initialize: selected %ld valid extension indices of %ld arrays\n", valididx.size(), alist.size() );
	arraylist_t v = ::selectArrays ( alist, valididx );

	// reset valididx
	for ( size_t i=0; i<valididx.size(); i++ )
		valididx[i]=i;

 
	if ( verbose>=3 )
		printf ( "   v %ld\n", v.size() );
	return v;
}

void processDepth ( const arraylist_t &goodarrays, depth_alg_t depthalg, depth_extend_t &dextend, depth_extend_sub_t &dextendsublight, int extensioncol, int verbose )
{
	//if (goodarrays.size()>0 ) 		printfd(": start: goodarrays[0].ncols %d\n", goodarrays[0].n_columns );
	
	depth_extend_sub_t dextendsub=dextendsublight;
	switch ( depthalg ) {
	case DEPTH_EXTENSIONS:

		if ( verbose>=3 ) {
			printf ( "depth_extend_array: calling depth_extend! %ld arrays, %ld/%ld extensions, extcol %d\n", goodarrays.size(), dextendsub.valididx.size(), dextend.extension_column_list.size(), extensioncol );
			ff();
		}
#ifdef DOOPENMP
//printfd("entry to depth_extend_omp: extesioncol %d\n", extensioncol );
		dextend.setposition ( extensioncol, -1, goodarrays.size() );
		depth_extend_omp ( goodarrays, dextend, dextendsub, extensioncol, 1 );
#else
		depth_extend ( goodarrays, dextend, dextendsub, extensioncol, 1 );
#endif
		//printf("depth_extend_array: calling depth_extend:done\n");

		break;
	case DEPTH_DIRECT: {
		if ( verbose>=1 ) {
			printf ( "processDepth: calling depth_extend_direct! %ld  arrays, %ld extensions, extcol %d\n", goodarrays.size(),dextend.extension_column_list.size(), extensioncol );
			ff();
		}

		OAextend oaextendDirect = dextend.oaextend;
		oaextendDirect.use_row_symmetry=1;
		oaextendDirect.extendarraymode=OAextend::APPENDFULL;
		oaextendDirect.checkarrays=1;

		depth_extend_hybrid ( goodarrays, dextend, extensioncol, oaextendDirect, 1 );
		//depth_extend_direct ( goodarrays, dextend, extensioncol, oaextendDirect, 1 );
	}
	break;
	default
			:
		printf ( "no such depth algoritm\n" );
		exit ( 1 );
	}
}


void depth_extend_hybrid ( const arraylist_t &alist,  depth_extend_t &dextend, int extcol, const OAextend &oaextendx, int verbose )
{

	std::string sp = depth_extend_logstring ( extcol-dextend.loglevelcol );
	if ( extcol>=dextend.ad->ncols )
		return;

	if ( verbose>=2 || 0 )
		printf ( "%sdepth_extend_hybrid column %d->%d: input %d arrays\n", sp.c_str(), extcol, extcol+1, ( int ) alist.size() );

	const arraydata_t *adfull = dextend.ad;

	// loop over all arrays
	for ( size_t i=0; i<alist.size(); i++ ) {
		array_link al = alist[i];
		int ps =alist[i].row_symmetry_group().permsize();
		
		printfd ( "### extcol %d: array %d: row group size %d\n", extcol, i, ps );

		dextend.setposition ( extcol, i, alist.size(), -1, -1 );

		if ( dextend.showprogress ( 1, extcol ) ) {
			printf ( "%sdepth_extend_direct: column %d, array %d/%d\n", sp.c_str(), extcol, ( int ) i, ( int ) alist.size() );
			ff();
		}

		if ( ps>=4000 ) {
			arraydata_t adlocal ( dextend.ad, extcol+1 );

			setloglevel ( SYSTEM );
			arraylist_t alocal;
			extend_array ( al.array,  &adlocal, extcol, alocal, oaextendx );

			dextend.counter->addNfound ( extcol+1, alocal.size() );
			dextend.arraywriter->writeArray ( alocal );

			depth_extend_direct ( alocal,  dextend, extcol+1, oaextendx, verbose );
		} else {
			depth_extend_t dextendloop ( dextend );
			dextendloop.setposition ( al.n_columns, i, alist.size(), 0, 0 );
			depth_extend_array ( al, dextendloop, *adfull, verbose, 0, i );			
		}
	}
}


void depth_extend_direct ( const arraylist_t &alist,  depth_extend_t &dextend, int extcol, const OAextend &oaextendx, int verbose )
{
	std::string sp = depth_extend_logstring ( extcol-dextend.loglevelcol );
	if ( extcol>=dextend.ad->ncols )
		return;

	if ( verbose>=2 || 0 )
		printf ( "%sdepth_extend column %d->%d: input %d arrays\n", sp.c_str(), extcol, extcol+1, ( int ) alist.size() );


	// loop over all arrays
	for ( size_t i=0; i<alist.size(); i++ ) {
		array_link al = alist[i];
		dextend.setposition ( extcol, i, alist.size(), -1, -1 );

		if ( verbose>=2 || extcol<=dextend.loglevelcol ) {
			// log if extcol is small enough
			printf ( "%sdepth_extend_direct column %d->%d, parse array %d/%d\n", sp.c_str(), extcol, extcol+1, ( int ) i, ( int ) alist.size() );
			ff();
		} else {
			if ( verbose &&  extcol<=dextend.loglevelcol+9 ) {
				// log at certain time intervals if extcol is small enough
				if ( dextend.showprogress ( 1, extcol ) ) {
					printf ( "%sdepth_extend_direct: column %d, array %d/%d\n", sp.c_str(), extcol, ( int ) i, ( int ) alist.size() );
					ff();
				}
			}
		}

		arraydata_t adlocal ( dextend.ad, extcol+1 );

		setloglevel ( SYSTEM );
		arraylist_t alocal;
		extend_array ( al.array,  &adlocal, extcol, alocal, oaextendx );

		dextend.counter->addNfound ( extcol+1, alocal.size() );
		//dextend.nfound[extcol+1]+=alocal.size();
		dextend.arraywriter->writeArray ( alocal );

		if ( extcol<dextend.ad->ncols-1 && alocal.size() >0 ) {
			if ( verbose>=3 ) {
				printf ( "%sdepth_extend column %d->%d: calling depth_extend_direct \n", sp.c_str(), extcol, extcol+1 );

			}
			depth_extend_direct ( alocal,  dextend, extcol+1, oaextendx, verbose );
		}

	}
}

void depth_extend_array ( const array_link &al, depth_extend_t &dextend, const arraydata_t &adfull,  int verbose, depth_extensions_storage_t *ds, int dsidx )
{
	OAextend oaextendx = dextend.oaextend;

	int extensioncol = al.n_columns;
	dextend.loglevelcol=al.n_columns;

	if ( extensioncol>5 )
		oaextendx.init_column_previous=INITCOLUMN_PREVIOUS;
	else
		oaextendx.init_column_previous=INITCOLUMN_ZERO;

	arraylist_t extensions0;
	arraylist_t goodarrays;

	int64_t ps = al.row_symmetry_group().permsize();
	// al.row_symmetry_group().show(2);

	depth_alg_t depthalg = DEPTH_DIRECT;
	depth_extend_sub_t dextendsub;

	if ( ps>20000 || ps>1999999 ) {
		// IDEA: enable caching of extensions at later stage!

		printfd ( "depth_extend_array: warning: direct extension due to large symmetry group!\n" );
		depthalg=DEPTH_DIRECT;

		OAextend oaextendDirect = oaextendx;
		oaextendDirect.use_row_symmetry=1;
		oaextendDirect.extendarraymode=OAextend::APPENDFULL;
		oaextendDirect.checkarrays=1;


		extend_array ( al.array,  &adfull, extensioncol, goodarrays, oaextendDirect );

	} else {
		//dextend.cache_extensions =1;
		depthalg = DEPTH_EXTENSIONS;

		/// extend the arrays without using the row symmetry property

		if ( 0 ) {
			setloglevel ( NORMAL );
			oaextendx.use_row_symmetry=0;

			extend_array ( al.array,  &adfull, extensioncol, extensions0, oaextendx );
			oaextendx.use_row_symmetry=1;
		}

		{
			// OPTIMIZE: use row_symmtry=1 and then add missing extensions
			extend_array ( al.array,  &adfull, extensioncol, extensions0, oaextendx );
		}

		dextendsub.resize ( extensions0.size() );
		// dextendsub.verbose=2; //
		dextend.extension_column_list = dextendsub.initialize ( extensions0, adfull, oaextendx );
		dextendsub.info();

		if ( 0 ) {

			for ( int i=0; i< ( int ) extensions0.size(); i++ ) {
				LMCreduction_t reduction ( &adfull );
				reduction.updateSDpointer ( extensions0[i] );

				// find reduced column index

				int pi = i;

				// results
				if ( dextendsub.lmctype[i]>=LMC_EQUAL ) {
					printf ( "array %d: lmc %d, lastcol %d, lastcol parent %d\n", i, dextendsub.lmctype[i], dextendsub.lastcol[i], dextendsub.lastcol[pi] );
				}
				//dextend.lmctype[k]=lmc;
				//      dextend.lastcol[k]=lc;
			}
		}

		// make selection
		goodarrays = dextendsub.selectArraysZ ( extensions0 );
	}

	dextend.counter->addNfound ( extensioncol+1, goodarrays.size() );
	dextend.arraywriter->writeArray ( goodarrays );
	dextend.maxArrayCheck();

	extensioncol++;

	// TODO: use row symmetry check to reduce arrays
	// TODO: use lastrow to reduce arrays (related to init_column_previous)

	//printfd ( "number of extensions: %zu\n", extensions0.size() );
	if ( ds!=0 ) {
		#pragma omp critical
		{
			ds->set ( dsidx, goodarrays, dextend.extension_column_list, depthalg, dextendsub );
		}
		return;
	}

	//printfd(" goodarrays[0].ncols %d\n", goodarrays[0].n_columns);
	processDepth ( goodarrays, depthalg, dextend, dextendsub, extensioncol, verbose );
}


/// helper function
void depth_extend_log ( int i, const arraylist_t &alist, int nn, depth_extend_t &dextend, int extcol, int verbose )
{
	//printfd("depth_extend_log: verbose %d, dextend.loglevelcol %d\n", verbose, dextend.loglevelcol);

	std::string sp = depth_extend_logstring ( extcol-dextend.loglevelcol );

	if ( verbose>=2 || extcol<=dextend.loglevelcol ) {
		// log if extcol is small enough
		printf ( "%sdepth_extend column %d->%d, parse array %d/%d (%d extend cols)\n", sp.c_str(), extcol, extcol+1, ( int ) i, ( int ) alist.size(), nn );
		ff();
	} else {
		//printf("verbose %d, extcol %d ~ %d\n", verbose, extcol, dextend.loglevelcol+1);
		if ( verbose ) {
			// log at certain time intervals if extcol is small enough
			if ( dextend.showprogress ( 1, extcol ) ) {
				// printf ( "%sdepth_extend: column %d, array %d/%d (%zu extend cols)\n", sp.c_str(), extcol, ( int ) i, ( int ) alist.size(), nn );
				ff();
				if ( i>2 ) {
					//   exit(0);
				}
			}

		}
	}
}

/// variation of depth_extend in which OpenMP is used
void depth_extend_omp ( const arraylist_t &alist,  depth_extend_t &dextend, depth_extend_sub_t &dextendsub, int extcol, int verbose, int nomp )
{

	const int dv=0;
	cprintf ( dv>=1, "depth_extend_omp! extcol %d\n", extcol );

	//		printfd(": extcol %d, al[0].n_columns %d ", extcol, alist[0].n_columns); dextend.ad->show();

	std::string sp = depth_extend_logstring ( extcol-dextend.loglevelcol );

	if ( verbose>=2 ) {
		printf ( "depth_extend: start: extcol %d, ad->ncols %d\n", dextend.ad->ncols, extcol );
	}
	if ( extcol>=dextend.ad->ncols )
		return;


	myassert ( dextend.extension_column_list.size() >=dextendsub.valididx.size(), "depth_extend: problem with valididx.size()" );

	int bi = 0; /// number of arrays for which branching takes place

	// loop over all arrays
	for ( size_t i=0; i<alist.size(); i++ ) {
		//printfd ( "i %d/%d\n", i, alist.size() );
		const array_link &al = alist[i];
#ifdef OADEBUG
		if ( al.n_columns+1 > dextend.ad->ncols ) {
			printf ( "depth_extend_omp: error extension has too many columns: extcol %d, al.n_columns %d, dextend.ad->ncols %d\n", extcol, al.n_columns, dextend.ad->ncols );
			exit ( 1 );
		}
#endif


		depth_extend_sub_t dlocal = dextendsub;	 // copy needed?
		size_t nn=dlocal.valididx.size();
		// dlocal.verbose=2;

		// make each thread log separately
		if ( alist.size() >1 ) {
			dextend.setposition ( extcol, i, alist.size(), nn, -1 );
		}

		depth_extend_log ( i, alist, nn, dextend,  extcol,  verbose );

		//printf("dextend.discardJ5 %d, extcol %d\n", dextend.discardJ5, extcol);
		if (dextend.discardJ5 >=0 and (dextend.discardJ5 <= extcol) ) {
			std::vector<int> j5 = al.Jcharacteristics(5);
			for(size_t i=0; i<j5.size(); i++) j5[i]=abs(j5[i]);
			int j5max = vectormax(j5, 0);
			
			if (j5max==al.n_rows) {
				if (dlocal.verbose>=2) {
				printf("depth_extend: discardJ5: extcol %d, j5max %d: setting number of extensions (%d) to zero\n", extcol, j5max, (int) nn);
				//dextend.showprogress ( 1, extcol );
				dextend.showsearchpath ( extcol );
				}
				dextend.discardJ5number+=nn;
			nn=0;	
			}
		}


		// 1. extend current array with all columns
		dlocal.resize ( nn );

		//      LMCreduction_t reduction ( &adlocal );
		//printf("  extcol: %d, ad.ncols %d\n", extcol, dextend.ad->ncols);
		arraydata_t adlocal ( dextend.ad, extcol+1 );
		arraydata_t adsub ( dextend.ad, extcol );

		LMCreduction_t reductionsub ( &adsub );

		// check possible extensions
		//printfd("## start of omp for loop: extcol %d: i %d: loop over %d elements\n", extcol, (int)i, (int)nn);
		//omp_set_num_threads(4);
		//#pragma omp parallel for
		for ( size_t j=0; j<nn; j++ ) {
			// printfd("j %d\n", j);
			if ( dlocal.verbose>=2 )
				printf ( "%sdepth_extend: col %d: j %ld: %d %ld\n", sp.c_str(), extcol, j,dextendsub.valididx[j], dextend.extension_column_list.size() );

			array_link ee = hstacklastcol ( al, dextend.extension_column_list[dextendsub.valididx[j]] );

			if ( verbose>=3 ) {
				printf ( "ee: " );
				ee.show();
			}

			//printfd("before strength check: "); ee.show(); dextend.ad->show();

			// 2. for each extension: check strength condition
			int b = strength_check ( * ( dextend.ad ), ee );

			dlocal.strengthcheck[j]=b;
			if ( dlocal.verbose>=2 )
				printf ( "%sarray %d: extension %d: strength check %d\n", sp.c_str(), ( int ) i, ( int ) j, b );

			if ( b ) {
				// do lmc check
				LMCreduction_t reduction ( &adlocal ); // TODO: place outside loop (only if needed)
				LMCreduction_t tmp = reduction;


				// make sure code is thread safe
				reduction.initStatic();

				double t0; double dt;
				lmc_t lmc;

				//reduction.reset();
				reduction.init_state=COPY;
				t0=get_time_ms();
				lmc =  LMCcheck ( ee, ( adlocal ), dextend.oaextend, reduction );
				dt=get_time_ms()-t0;

				reduction.releaseStatic();

				#pragma omp critical
				{
					int lc=reduction.lastcol;
					dlocal.lmctype[j]=lmc;
					dlocal.lastcol[j]=lc;
					if ( dlocal.verbose>=2 && lmc>=LMC_EQUAL )
						printf ( "%sarray %d: extension %d: strength check %d, lmc %d, dt %.3f\n", sp.c_str(), ( int ) i, ( int ) j, b, lmc, dt );

				} // OMP_CRITIAL
			} else  {
				dlocal.lmctype[j]=LMC_LESS;
			}
		} // end of omp parallel for
		//printfd("end of omp for loop\n");

		std::vector<int> localvalididx = dlocal.updateExtensionPointers ( extcol );

		arraylist_t alocal = dlocal.selectArraysXX ( al, dextend.extension_column_list );
		if ( verbose>=2 ) {
			printf ( "%sdepth_extend column %d->%d: adding %ld/%ld arrays\n", sp.c_str(), extcol, extcol+1, alocal.size(), dextendsub.valididx.size() );

		}

#ifdef DOOPENMP
		dextend.setpositionGEC ( extcol, alocal.size() );
#else
		dextend.setposition ( extcol, i, alist.size(), nn, alocal.size() );
#endif

		if ( extcol==11 && 0 ) {
			printf ( " dextend.setposition: %d, %d, %d\n", extcol, ( int ) i, ( int ) alist.size() );
		}
		dextend.counter->addNfound ( extcol+1, alocal.size() );
		dextend.arraywriter->writeArray ( alocal );
		dextend.maxArrayCheck();

		if ( extcol<dextend.ad->ncols-1 && alocal.size() >0 ) {
			if ( verbose>=3 ) {
				printf ( "%sdepth_extend column %d->%d: calling depth extend with %ld array extensions\n", sp.c_str(), extcol, extcol+1, localvalididx.size() );
			}

			cprintf ( dv>=1, "  # depth_extend column : array %d/%d: nn %d, calling depth extend with %ld array extensions\n", ( int ) i, ( int ) alist.size(), nn, localvalididx.size() );

			//printf ( "# %sdepth_extend column %d->%d: calling depth extend with %ld array extensions\n", sp.c_str(), extcol, extcol+1, dlocal2.valididx.size() );
			//printf("calling depth_extend: %d %d\n",  alocal[0].n_columns, extcol+1 );

			if ( nomp>=1 || extcol>=13 || alocal.size() <=3 || extcol<=9 ) {
				if ( dv>=1 )
					printf ( "depth_extend_omp: extcol %d, i %ld/%ld, recursive on %ld arrays!\n ", extcol, i, alist.size(), alocal.size() );
				// recursive method
				depth_extend_sub_t dlocal2 ( 0 );
				dlocal2.valididx=localvalididx;
				depth_extend_omp ( alocal,  dextend, dlocal2, extcol+1, verbose, nomp );
			} else {
				// parallel method
				const size_t alocalsize = alocal.size();
				cprintf ( dv>=1, "depth_extend_omp: extcol %d, i %ld/%ld, running parallel omp on %ld arrays!\n ", extcol, i, alist.size(), alocal.size() );
				
				//printfd ( "  openmp: num threads %d, omp_get_dynamic() %d, omp_get_nested() %d, omp_get_max_active_levels() %d\n", omp_get_max_threads(), omp_get_dynamic(), omp_get_nested(), omp_get_max_active_levels() );
				//printfd ( " using omp loop: narrays %d, extcol %d\n", alocalsize, extcol) ;
				
//				omp_set_num_threads(4);
				#pragma omp parallel for schedule(dynamic,1)
				//#pragma omp parallel for num_threads(4) schedule(dynamic,1)
				for ( int i=0; i<(int)alocalsize; i++ ) {
					depth_extend_sub_t dlocal2 ( 0 );
					dlocal2.valididx=localvalididx;

					//continue;
					arraylist_t alocalt;
					alocalt.push_back ( alocal[i] );
					//dextend.setposition(extcol+1, i, alocal.size() );
					dextend.setposition ( extcol+1, i, alocal.size(), dextendsub.valididx.size(), -1 );
					depth_extend_t dextendloop ( dextend );
					dextendloop.setposition ( extcol+1, i, alocal.size(), dextendsub.valididx.size(), -1 );

					//if (i==0)	printf ( "depth %d: array %zu/%d: thread %d/%d, omp_get_level() %d, omp_get_nested() %d\n", extcol, i, (int)alocalsize , omp_get_thread_num(), omp_get_num_threads(), omp_get_level(), omp_get_nested() );

					depth_extend_omp ( alocalt,  dextendloop, dlocal2, extcol+1, verbose, nomp+1 );
				}
			}
		}

		if ( alocal.size() >0 ) {
			bi++;
		}
	}
	cprintf ( dv>=1, "depth_extend: extcol %d: input %zu arrays, nn %d, branching %d/%zu\n", extcol, alist.size(), ( int ) dextendsub.valididx.size(), bi, alist.size() );
}



// kate: indent-mode cstyle; indent-width 4; replace-tabs off; tab-width 4; 