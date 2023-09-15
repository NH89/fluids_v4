#include <assert.h>
//#include <cuda.h>
#include <CL/cl.h>
#include <unistd.h>
//#include <curand_kernel.h> // ../cuda-11.2/targets/x86_64-linux/include/
#include "fluid_system.h"
#include "fluid_system.cpp"
#include "fileCL_IO.cpp"
#include <CL/cl_ext.h>

#define UINT_MAX 65535



int iDivUp (int a, int b) {
    return (a % b != 0) ? (a / b + 1) : (a / b);
}

cl_int FluidSystem::clMemsetD32(cl_mem buffer, int value, size_t count) {
    cl_int err;

    // Set kernel arguments
    err  = clSetKernelArg(m_Kern[FUNC_MEMSET32D], 0, sizeof(cl_mem), &buffer);
    err |= clSetKernelArg(m_Kern[FUNC_MEMSET32D], 1, sizeof(int), &value);
    clCheck(err, "clMemsetD32", "clSetKernelArg", NULL, mbDebug);

    // Enqueue kernel
    err = clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_MEMSET32D], 1, NULL, &count, NULL, 0, NULL, NULL);
    clCheck(err, "clMemsetD32", "clEnqueueNDRangeKernel", NULL, mbDebug);

    // Ensure all commands have finished
    clFinish(m_queue);

    // Release kernel
    clReleaseKernel(m_Kern[FUNC_MEMSET32D]);

    return CL_SUCCESS;
}

void computeNumBlocks (int numPnts, int minThreads, int &numBlocks, int &numThreads){
    numThreads = min( minThreads, numPnts );
    numBlocks = (numThreads==0) ? 1 : iDivUp ( numPnts, numThreads );
}

void FluidSystem::TransferToTempCL ( int buf_id, int sz ){
    //clCheck ( cuMemcpyDtoD ( gpuVar(&m_FluidTemp, buf_id), gpuVar(&m_Fluid, buf_id), sz ), "TransferToTempCL", "cuMemcpyDtoD", "m_FluidTemp", mbDebug);
    clCheck ( clEnqueueCopyBuffer(m_queue, gpuVar(&m_Fluid, buf_id), gpuVar(&m_FluidTemp, buf_id), 0, 0, sz, 0, NULL, NULL), "TransferToTempCL", "clEnqueueCopyBuffer", "m_FluidTemp", mbDebug);
}

void FluidSystem::InsertParticlesCL(uint* gcell, uint* gndx, uint* gcnt) {

    // first zero the counters
    cl_int err;
    err = clEnqueueFillBuffer(m_queue, gpuVar(&m_Fluid, FGRIDCNT), 0, sizeof(int), 0, m_GridTotal * sizeof(int), 0, NULL, NULL);

    cout << err;

    /*
    err = clEnqueueFillBuffer(m_queue, gpuVar(&m_Fluid, FGRIDOFF), 0, sizeof(int), 0, m_GridTotal * sizeof(int), 0, NULL, NULL);

    err = clEnqueueFillBuffer(m_queue, gpuVar(&m_Fluid, FGRIDCNT_ACTIVE_GENES), 0, sizeof(uint[NUM_GENES]), 0, m_GridTotal * sizeof(uint[NUM_GENES]), 0, NULL, NULL);
    err = clEnqueueFillBuffer(m_queue, gpuVar(&m_Fluid, FGRIDOFF_ACTIVE_GENES), 0, sizeof(uint[NUM_GENES]), 0, m_GridTotal * sizeof(uint[NUM_GENES]), 0, NULL, NULL);

    // Set long list to sort all particles.
    computeNumBlocks(m_FParams.pnum, m_FParams.threadsPerBlock, m_FParams.numBlocks, m_FParams.numThreads); // particles
    // launch kernel "InsertParticles"
    void* args[1] = {&mMaxPoints}; //&mNumPoints
    err = clEnqueueNDRangeKernel(m_queue,
                                 m_Kern[FUNC_INSERT],
                                 1,
                                 NULL,
                                 (const size_t*)&m_FParams.numBlocks,
                                 (const size_t*)&m_FParams.numThreads,
                                 0,
                                 NULL,
                                 NULL);

    if (m_FParams.debug > 1)
        cout << "\n#######\nCalling InsertParticles kernel: args[1] = {" << mNumPoints << "}, mMaxPoints=" << mMaxPoints
             << "\t m_FParams.numBlocks=" << m_FParams.numBlocks << ", m_FParams.numThreads=" << m_FParams.numThreads << " \t" << std::flush;

    // Transfer data back if requested (for validation)
    if (gcell != 0x0) {
        err = clEnqueueReadBuffer(m_queue,
                                  gpuVar(&m_Fluid, FGCELL),
                                  CL_TRUE,
                                  0,
                                  mNumPoints * sizeof(uint),
                                  gcell,
                                  0,
                                  NULL,
                                  NULL);
        err = clEnqueueReadBuffer(m_queue,
                                  gpuVar(&m_Fluid, FGNDX),
                                  CL_TRUE,
                                  0,
                                  mNumPoints * sizeof(uint),
                                  gndx,
                                  0,
                                  NULL,
                                  NULL);
        err = clEnqueueReadBuffer(m_queue,
                                  gpuVar(&m_Fluid, FGRIDCNT),
                                  CL_TRUE,
                                  0,
                                  m_GridTotal * sizeof(uint),
                                  gcnt,
                                  0,
                                  NULL,
                                  NULL);

        clFinish(m_queue);
    }
    if (m_debug > 4) {
        if (m_FParams.debug > 1)
            cout << "\nSaving (FGCELL) InsertParticlesCL: (particleIdx, cell) , mMaxPoints=" << mMaxPoints << "\t" << std::flush;
        err = clEnqueueReadBuffer(m_queue,
                                  gpuVar(&m_Fluid, FGCELL),
                                  CL_TRUE,
                                  0,
                                  sizeof(uint[mMaxPoints]),
                                  bufI(&m_Fluid, FGCELL),
                                  0,
                                  NULL,
                                  NULL);
        // Has to be implemented again. CUCLCUCL
        //SaveUintArray(bufI(&m_Fluid, FGCELL), mMaxPoints, "InsertParticlesCL__bufI(&m_Fluid, FGCELL).csv");
    }*/
                                                                            if(verbosity>0) cout << "-----InsertParticlesCL finished-----\n\n" << flush;
}

 void FluidSystem::PrefixSumCellsCL (int zero_offsets) {


    cl_int status;
    cl_event writeEvt;
    // Prefix Sum - determine grid offsets
    int t_blockSize = SCAN_BLOCKSIZE << 1;

    size_t t_fixedNumElem = static_cast<size_t>(1);
    const size_t* fixedNumElem = &t_fixedNumElem;
    size_t sizeValue0 = static_cast<size_t>(t_blockSize);
    const size_t* blockSize = &sizeValue0;

    int t_numElem1 = m_GridTotal;
    cout << m_GridTotal;
    size_t sizeValue1 = static_cast<size_t>(t_numElem1);
    const size_t* numElem1 = &sizeValue1;

    int t_numElem2 = int ((*numElem1) / (*blockSize)) + 1;
    size_t sizeValue2 = static_cast<size_t>(t_numElem2);
    const size_t* numElem2 = &sizeValue2;

    int t_numElem3 = int ((*numElem2) / (*blockSize)) + 1;
    size_t sizeValue3 = static_cast<size_t>(t_numElem3);
    const size_t* numElem3 = &sizeValue3;

    int t_threads = SCAN_BLOCKSIZE;
    size_t sizeValue4 = static_cast<size_t>(t_threads);
    const size_t* threads = &sizeValue4;


    int zon = 1;

    cl_mem array1 = gpuVar(&m_Fluid, FGRIDCNT);
    cl_mem scan1 = gpuVar(&m_Fluid, FGRIDOFF);
    cl_mem array2 = gpuVar(&m_Fluid, FAUXARRAY1);
    cl_mem scan2 = gpuVar(&m_Fluid, FAUXSCAN1);
    cl_mem array3 = gpuVar(&m_Fluid, FAUXARRAY2);
    cl_mem scan3 = gpuVar(&m_Fluid, FAUXSCAN2);


    #ifndef xlong
        typedef unsigned long long xlong;
    #endif

    cl_mem array0 = gpuVar(&m_Fluid,FGRIDCNT_ACTIVE_GENES);
    cl_mem scan0 = gpuVar(&m_Fluid,FGRIDOFF_ACTIVE_GENES);
    cout << "____\n\n\n";

    for (int gene = 0; gene < NUM_GENES; gene++) {
    size_t offset_array0 = gene * t_numElem1 * sizeof(int);
    size_t offset_scan0 = gene * t_numElem1 * sizeof(int);

    status = clEnqueueWriteBuffer(m_queue, m_FPrefixDevice, CL_FALSE, 0, 16 * sizeof(offset_array0), &offset_array0, 0, NULL, &writeEvt);						// WriteBuffer param_buf ##########


    int pattern = 0;


        // Set kernel arguments
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 0, sizeof(cl_mem), &array0),            "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM1", mbDebug);
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 1, sizeof(cl_mem), &scan0),             "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM2", mbDebug);
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 2, sizeof(cl_mem), &array2),            "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM3", mbDebug);
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 3, sizeof(size_t), &offset_array0),     "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM6", mbDebug);
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 4, sizeof(size_t), &offset_scan0),     "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM7", mbDebug);
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 5, sizeof(int), &t_numElem1),           "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM4", mbDebug);
        clCheck(clSetKernelArg(m_Kern[FUNC_FPREFIXSUM], 6, sizeof(int), &zero_offsets),         "YourFunction", "clSetKernelArg", "FUNC_FPREFIXSUM5", mbDebug);



//         clCheck(clEnqueueFillBuffer(m_queue, scan0, &pattern, sizeof(pattern), 0, offset_scan0 * sizeof(int), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueFillBuffer", "FGRIDCNT", mbDebug);
//         clCheck(clEnqueueFillBuffer(m_queue, array0, &pattern, sizeof(pattern), 0, offset_array0 * sizeof(int), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueFillBuffer", "FGRIDCNT", mbDebug);
//         clCheck(clEnqueueFillBuffer(m_queue, scan2, &pattern, sizeof(pattern), 0, t_numElem2 * sizeof(int), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueFillBuffer", "FGRIDCNT", mbDebug);
//         clCheck(clEnqueueFillBuffer(m_queue, array3, &pattern, sizeof(pattern), 0, t_numElem3 * sizeof(int), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueFillBuffer", "FGRIDCNT", mbDebug);
//         clCheck(clEnqueueFillBuffer(m_queue, scan3, &pattern, sizeof(pattern), 0, t_numElem3 * sizeof(int), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueFillBuffer", "FGRIDCNT", mbDebug);

    //void* argsA[5] = {&array1, &scan1, &array2, &numElem1, &zero_offsets};

    status = clFlush(m_queue); 				if (status != CL_SUCCESS)	{ cout << "\nclFlush(m_queue) status = " << checkerror(status) <<"\n"<<flush; exit_(status);}
	status = clFinish(m_queue); 			if (status != CL_SUCCESS)	{ cout << "\nclFinish(m_queue)="		 <<checkerror(status)  <<"\n"<<flush; exit_(status);}
    clCheck(clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_FPREFIXSUM], 1, 0, &global_work_size, 0, 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueNDRangeKernel", "FUNC_PREFIXSUM", mbDebug);


    printf("\nPREFIX SUM done.\n");
//     //void* argsB[5] = {&array2, &scan2, &array3, &numElem2, &zon};
//     clCheck(clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_FPREFIXSUM], 1, NULL, numElem3, threads, 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueNDRangeKernel", "FUNC_PREFIXSUM", mbDebug);
//
//     if ((*numElem3) > 1) {
//         cl_mem nptr = {0};
//         //void* argsC[5] = {&array3, &scan3, &nptr, &numElem3, &zon};
//         clCheck(clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_FPREFIXSUM], 1, NULL, fixedNumElem, threads, 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueNDRangeKernel", "FUNC_PREFIXFIXUP", mbDebug);
//
//         //void* argsD[3] = {&scan2,&scan3,&numElem2};
//         clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXUP],1, NULL, numElem3, threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXFIXUP",mbDebug);
//     }
//
// //     void* argsE[3] = {&scan1,&scan2,&numElem1};
//     clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXUP],1,NULL, numElem2, threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXFIXUP",mbDebug);

    // Loop to PrefixSum the Dense Lists - NB by doing one gene at a time,
    // we reuse the FAUX* arrays & scans.
    // For each gene,
    // input FGRIDCNT_ACTIVE_GENES[gene*m_GridTotal],
    // output FGRIDOFF_ACTIVE_GENES[gene*m_GridTotal]

/*

//         void* argsA[5] = {&array1,&scan1,&array2,&numElem1,&zero_offsets};
        clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXSUM],1,NULL,numElem2,threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXSUM",mbDebug);

//         void* argsB[5] = {&array2,&scan2,&array3,&numElem2,&zon};
        clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXSUM],1,NULL,numElem3,threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXSUM",mbDebug);

        if ((*numElem3) > 1) {
            cl_mem nptr = {0};
//             void* argsC[5] = {&array3,&scan3,&nptr,&numElem3,&zon};
            clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXSUM],1,NULL,fixedNumElem,threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXFIXUP",mbDebug);

//             void* argsD[3] = {&scan2,&scan3,&numElem2};
            clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXUP],1,NULL,numElem3,threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXFIXUP",mbDebug);
        }

//         void* argsE[3] = {&scan1,&scan2,&numElem1};
        clCheck(clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_FPREFIXUP],1,NULL,numElem2,threads,0,NULL,NULL),"PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_PREFIXFIXUP",mbDebug);*/
    }

    printf("\nPhase 3 done.\n");

    int num_lists = NUM_GENES,
    length = FDENSE_LIST_LENGTHS,
    fgridcnt = FGRIDCNT_ACTIVE_GENES,
    fgridoff = FGRIDOFF_ACTIVE_GENES;
    size_t t_NUM_GENES_CONST = static_cast<size_t>(NUM_GENES);
    const size_t* NUM_GENES_CONST = &t_NUM_GENES_CONST;

//     void* argsF[4] = {&num_lists,&length,&fgridcnt,&fgridoff};
    status = clFlush(m_queue); 				if (status != CL_SUCCESS)	{ cout << "\nclFlush(m_queue) status = " << checkerror(status) <<"\n"<<flush; exit_(status);}
	status = clFinish(m_queue); 			if (status != CL_SUCCESS)	{ cout << "\nclFinish(m_queue)="		 <<checkerror(status)  <<"\n"<<flush; exit_(status);}

    clCheck(    clEnqueueNDRangeKernel(m_queue,m_Kern[FUNC_TALLYLISTS], NUM_GENES, 0, &global_work_size, 0, 0, NULL, NULL), "PrefixSumCellsCL","clEnqueueNDRangeKernel","FUNC_TALLYLISTS",mbDebug);


    status = clFlush(m_queue); 				if (status != CL_SUCCESS)	{ cout << "\nclFlush(m_queue) status = " << checkerror(status) <<"\n"<<flush; exit_(status);}
	status = clFinish(m_queue); 			if (status != CL_SUCCESS)	{ cout << "\nclFinish(m_queue)="		 <<checkerror(status)  <<"\n"<<flush; exit_(status);}

    clCheck(    clEnqueueReadBuffer(m_queue,gpuVar(&m_Fluid,FDENSE_LIST_LENGTHS),CL_TRUE,0,sizeof(uint[NUM_GENES]),bufI(&m_Fluid, FDENSE_LIST_LENGTHS),0,NULL,NULL),    "PrefixSumCellsCL","clEnqueueReadBuffer","FDENSE_LIST_LENGTHS",mbDebug);

    uint* densebuff_len = bufI(&m_Fluid, FDENSE_BUF_LENGTHS);
    uint* denselist_len = bufI(&m_Fluid, FDENSE_LIST_LENGTHS);

    for (int gene = 0; gene < NUM_GENES; gene++) {
        if (denselist_len[gene] > densebuff_len[gene]) {
            if (m_FParams.debug > 1)
                printf("\n\nPrefixSumCellsCL: enlarging densebuff_len[%u], gpuptr(&m_Fluid, FDENSE_LIST_LENGTHS)[gene]=%p .\t", gene, gpuptr(&m_Fluid, FDENSE_LIST_LENGTHS)[gene]);
            while (denselist_len[gene] > densebuff_len[gene])
                densebuff_len[gene] *= 4;
            //AllocateBufferDenseLists(gene, sizeof(uint), bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene], FDENSE_LISTS);
        }
    }

    clCheck(clEnqueueWriteBuffer(m_queue, gpuVar(&m_Fluid, FDENSE_LISTS), CL_TRUE, 0, NUM_GENES * sizeof(cl_mem), bufC(&m_Fluid, FDENSE_LISTS), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueWriteBuffer", "FDENSE_LISTS", mbDebug);
    clCheck(clEnqueueWriteBuffer(m_queue, gpuVar(&m_Fluid, FDENSE_BUF_LENGTHS), CL_TRUE, 0, NUM_GENES * sizeof(cl_mem), bufC(&m_Fluid, FDENSE_BUF_LENGTHS), 0, NULL, NULL), "PrefixSumCellsCL", "clEnqueueWriteBuffer", "FDENSE_BUF_LENGTHS", mbDebug);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //if (m_FParams.debug > 1) {                                                                                                      //
    //    std::cout << "\nChk: PrefixSumCellsCL 4" << std::flush;                                                                     //
    //    for (int gene = 0; gene < NUM_GENES; gene++) {                                                                              //
    //        std::cout << "\ngene list_length[" << gene << "]=" << bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene] << "\t" << std::flush;  //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //}
    //}

}

void FluidSystem::CountingSortFullCL ( Vector3DF* ppos ){
    if (m_FParams.debug>1) std::cout << "\nCountingSortFullCL()1: mMaxPoints="<<mMaxPoints<<", mNumPoints="<<mNumPoints<<",\tmActivePoints="<<mActivePoints<<".\n"<<std::flush;

    // get number of active particles & set short lists for later kernels
    int grid_ScanMax = (m_FParams.gridScanMax.y * m_FParams.gridRes.z + m_FParams.gridScanMax.z) * m_FParams.gridRes.x + m_FParams.gridScanMax.x;

    //clCheck( cuMemcpyDtoH ( &mNumPoints,  gpuVar(&m_Fluid, FGRIDOFF)+(m_GridTotal-1/*grid_ScanMax+1*/)*sizeof(int), sizeof(int) ), "CountingSortFullCL1", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);

    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FGRIDOFF), CL_TRUE, (m_GridTotal-1)*sizeof(int), sizeof(int), &mNumPoints, 0, NULL, NULL), "CountingSortFullCL1", "clEnqueueReadBuffer", "FGRIDOFF", mbDebug);

    //clCheck( cuMemcpyDtoH ( &mActivePoints,  gpuVar(&m_Fluid, FGRIDOFF)+(grid_ScanMax/*-1*/)*sizeof(int), sizeof(int) ), "CountingSortFullCL2", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);

    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FGRIDOFF), CL_TRUE, grid_ScanMax*sizeof(int), sizeof(int), &mActivePoints, 0, NULL, NULL), "CountingSortFullCL2", "clEnqueueReadBuffer", "FGRIDOFF", mbDebug);
    /*
    int totalPoints = 0;
    clCheck( cuMemcpyDtoH ( &totalPoints,  gpuVar(&m_Fluid, FGRIDOFF)+(m_GridTotal)*sizeof(int), sizeof(int) ), "CountingSortFullCL3", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);
    std::cout<<"\nCountingSortFullCL(): totalPoints="<<totalPoints<<std::flush;
    */
    m_FParams.pnumActive = mActivePoints;                                     // TODO eliminate duplication of information & variables between fluid.h and fluid_system.h
    //clCheck ( cuMemcpyHtoD ( clFParams,	&m_FParams, sizeof(FParams) ), "CountingSortFullCL3", "cuMemcpyHtoD", "clFParams", mbDebug); // seems the safest way to update fparam.pnumActive on device.

    clCheck( clEnqueueWriteBuffer(m_queue, m_FParamDevice, CL_TRUE, 0, sizeof(FParams), &m_FParams, 0, NULL, NULL), "CountingSortFullCL3", "clEnqueueWriteBuffer", "clFParams", mbDebug);

    if (m_FParams.debug>1) std::cout<<"\nCountingSortFullCL()2: mMaxPoints="<<mMaxPoints<<" mNumPoints="<<mNumPoints<<",\tmActivePoints="<<mActivePoints<<",  m_GridTotal="<<m_GridTotal<<", grid_ScanMax="<<grid_ScanMax<<"\n"<<std::flush;

    // Transfer particle data to temp buffers
    //  (gpu-to-gpu copy, no sync needed)
    //TransferToTempCL ( FPOS,		mMaxPoints *sizeof(Vector3DF) );    // NB if some points have been removed, then the existing list is no longer dense,
    //TransferToTempCL ( FVEL,		mMaxPoints *sizeof(Vector3DF) );    // hence must use mMaxPoints, not mNumPoints
    //TransferToTempCL ( FVEVAL,	mMaxPoints *sizeof(Vector3DF) );    // { Could potentially use (old_mNumPoints + mNewPoints) instead of mMaxPoints}
    TransferToTempCL ( FFORCE,	mMaxPoints *sizeof(Vector3DF) );    // NB buffers are declared and initialized on mMaxPoints.
    TransferToTempCL ( FPRESS,	mMaxPoints *sizeof(float) );
    TransferToTempCL ( FDENSITY,	mMaxPoints *sizeof(float) );
    TransferToTempCL ( FCLR,		mMaxPoints *sizeof(uint) );
    TransferToTempCL ( FAGE,		mMaxPoints *sizeof(uint) );
    TransferToTempCL ( FGCELL,	mMaxPoints *sizeof(uint) );
    TransferToTempCL ( FGNDX,		mMaxPoints *sizeof(uint) );

    // extra data for morphogenesis
    TransferToTempCL ( FELASTIDX,		mMaxPoints *sizeof(uint[BOND_DATA]) );
    TransferToTempCL ( FPARTICLEIDX,	mMaxPoints *sizeof(uint[BONDS_PER_PARTICLE *2]) );
    TransferToTempCL ( FPARTICLE_ID,	mMaxPoints *sizeof(uint) );
    TransferToTempCL ( FMASS_RADIUS,	mMaxPoints *sizeof(uint) );
    TransferToTempCL ( FNERVEIDX,		mMaxPoints *sizeof(uint) );
    TransferToTempCL ( FCONC,		    mMaxPoints *sizeof(float[NUM_TF]) );
    TransferToTempCL ( FEPIGEN,	    mMaxPoints *sizeof(uint[NUM_GENES]) );

    // debug chk
    //clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FEPIGEN), gpuVar(&m_FluidTemp, FEPIGEN),	mMaxPoints *sizeof(uint[NUM_GENES]) ), "CountingSortFullCL4", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
    //SaveUintArray_2D( bufI(&m_Fluid, FEPIGEN), mMaxPoints, NUM_GENES, "CountingSortFullCL__m_FluidTemp.bufI(FEPIGEN)2.csv" );

    // reset bonds and forces in fbuf FELASTIDX, FPARTICLEIDX and FFORCE, required to prevent interference between time steps,
    // because these are not necessarily overwritten by the FUNC_COUNTING_SORT kernel.
    clFinish (m_queue);    // needed to prevent colision with previous operations

    float max_pos = max(max(m_Vec[PVOLMAX].x, m_Vec[PVOLMAX].y), m_Vec[PVOLMAX].z);
    uint * uint_max_pos = (uint*)&max_pos;
    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FPOS), *uint_max_pos, mMaxPoints * 3 ),  "CountingSortFullCL", "cuMemsetD32", "FELASTIDX",   mbDebug);

    //cout<<"\nCountingSortFullCL: m_Vec[PVOLMAX]=("<<m_Vec[PVOLMAX].x<<", "<<m_Vec[PVOLMAX].y<<", "<<m_Vec[PVOLMAX].z<<"),  max_pos = "<< max_pos <<std::flush;
    // NB resetting  gpuVar(&m_Fluid, FPOS)  ensures no zombie particles. ?hopefully?

    // Define the variables
//     struct Var {
//         cl_mem buffer;
//         int value;
//         size_t size;
//         const char* name;
//     };
//
//     Var vars[] = {
//         {NULL, (double)*uint_max_pos, (size_t)(mMaxPoints * 3), "FPOS"},
//         {NULL, UINT_MAX, mMaxPoints * BOND_DATA, "FELASTIDX"},
//         {NULL, UINT_MAX, mMaxPoints * BONDS_PER_PARTICLE * 2, "FPARTICLEIDX"},
//         {NULL, UINT_MAX, mMaxPoints, "FPARTICLE_ID"},
//         {NULL, 0.0, mMaxPoints * 3, "FFORCE"},
//         {NULL, 0.0, mMaxPoints * NUM_TF, "FCONC"},
//         {NULL, 0.0, mMaxPoints * NUM_GENES, "FEPIGEN"}
//     };
//
//     // Iterate over the variables
//     for (int i = 0; i < sizeof(vars) / sizeof(Var); i++) {
//         // Create buffer
//         vars[i].buffer = clCreateBuffer(m_context, CL_MEM_READ_WRITE,
//                                         sizeof(int) * vars[i].size,
//                                         NULL, &err);
//         clCheck(err, "CountingSortFullCL", "clCreateBuffer", vars[i].name, mbDebug);
//
//         // Set kernel arguments
//         err  = clSetKernelArg(m_Kern[FUNC_MEMSET32D], 0, sizeof(cl_mem), &vars[i].buffer);
//         err |= clSetKernelArg(m_Kern[FUNC_MEMSET32D], 1, sizeof(int), &vars[i].value);
//         clCheck(err, "CountingSortFullCL", "clSetKernelArg", vars[i].name, mbDebug);
//
//         // Determine global size
//         size_t global_size = vars[i].size;
//
//         // Enqueue kernel
//         err = clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_MEMSET32D], 1,
//                                     NULL,
//                                     &global_size,
//                                     NULL,
//                                     0,
//                                     NULL,
//                                     NULL);
//         clCheck(err, "CountingSortFullCL", "clEnqueueNDRangeKernel", vars[i].name, mbDebug);
//     }

    // Ensure all commands have finished
    clFinish(m_queue);

    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FELASTIDX),    UINT_MAX,  mMaxPoints * BOND_DATA              ),  "CountingSortFullCL", "cuMemsetD32", "FELASTIDX",    mbDebug);
    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FPARTICLEIDX), UINT_MAX,  mMaxPoints * BONDS_PER_PARTICLE *2  ),  "CountingSortFullCL", "cuMemsetD32", "FPARTICLEIDX", mbDebug);
    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FPARTICLE_ID), UINT_MAX,  mMaxPoints                          ),  "CountingSortFullCL", "cuMemsetD32", "FPARTICLEIDX", mbDebug);
    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FFORCE),      (uint)0.0,  mMaxPoints * 3 /* ie num elements */),  "CountingSortFullCL", "cuMemsetD32", "FFORCE",       mbDebug);
    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FCONC),             0.0,  mMaxPoints * NUM_TF                 ),  "CountingSortFullCL", "cuMemsetD32", "FCONC",        mbDebug);
    clCheck ( clMemsetD32 ( gpuVar(&m_Fluid, FEPIGEN),     (uint)0.0,  mMaxPoints * NUM_GENES              ),  "CountingSortFullCL", "cuMemsetD32", "FEPIGEN",      mbDebug);
    clFinish (m_queue);    // needed to prevent colision with previous operations

    // Reset grid cell IDs
    // clCheck(cuMemsetD32(gpuVar(&m_Fluid, FGCELL), GRID_UNDEF, numPoints ), "cuMemsetD32(Sort)");
    void* args[1] = { &mMaxPoints };
    for (int i = 0; i < sizeof(args) / sizeof(args[0]); i++) {clCheck(clSetKernelArg(m_Kern[FUNC_COUNTING_SORT], i, sizeof(args[i]), &args[i]), "CountingSortFullCL5", "clSetKernelArg", "FUNC_COUNTING_SORT", mbDebug);}

    // Determine global and local work sizes
    size_t global_work_size = m_FParams.numBlocks * m_FParams.numThreads;
    size_t local_work_size = m_FParams.numThreads;

    //clCheck ( cuLaunchKernel ( m_Kern[FUNC_COUNTING_SORT], m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL),
    //          "CountingSortFullCL5", "cuLaunch", "FUNC_COUNTING_SORT", mbDebug );

    clCheck ( clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_COUNTING_SORT], 1, NULL, &global_work_size, &local_work_size, 0, NULL, NULL),
              "CountingSortFullCL5", "cuLaunch", "FUNC_COUNTING_SORT", mbDebug );
    // Having sorted the particle data, we can start using a shortened list of particles.
    // NB have to reset to long list at start of time step.
    computeNumBlocks ( m_FParams.pnumActive, m_FParams.threadsPerBlock, m_FParams.numBlocks, m_FParams.numThreads);				// particles

    if (m_FParams.debug>1) std::cout<<"\n CountingSortFullCL : FUNC_COUNT_SORT_LISTS\n"<<std::flush;
    // countingSortDenseLists ( int pnum ) // NB launch on bins not particles.
    int blockSize = SCAN_BLOCKSIZE/2 << 1;
    int numElem1 = m_GridTotal;
    int numElem2 = 2*  int( numElem1 / blockSize ) + 1;
    int threads = SCAN_BLOCKSIZE/2;
    clFinish (m_queue);                             // NB threads/2 required on GTX970m

    // Set kernel arguments, assuming args is an array of arguments for the kernel
    for (int i = 0; i < sizeof(args) / sizeof(args[0]); i++) {clCheck(clSetKernelArg(m_Kern[FUNC_COUNT_SORT_LISTS], i, sizeof(args[i]), &args[i]), "CountingSortFullCL7", "clSetKernelArg", "FUNC_COUNT_SORT_LISTS", mbDebug);}

    // Determine global and local work sizes
    int global_work_size_int = numElem2 * threads;
    size_t global_work_size2 = (size_t)global_work_size_int;
    size_t local_work_size2 = threads;

    // Enqueue kernel
    clCheck(clEnqueueNDRangeKernel(m_queue, m_Kern[FUNC_COUNT_SORT_LISTS], 1, NULL, &global_work_size2, &local_work_size2, 0, NULL, NULL), "CountingSortFullCL7", "clEnqueueNDRangeKernel", "FUNC_COUNT_SORT_LISTS", mbDebug);

    clFinish (m_queue);

    if(m_FParams.debug>3){// debug chk
        std::cout<<"\n### Saving UintArray .csv files."<<std::flush;

        //clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FEPIGEN), gpuVar(&m_FluidTemp, FEPIGEN),	mMaxPoints *sizeof(uint[NUM_GENES]) ), "CountingSortFullCL8", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
        //clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_FluidTemp, FEPIGEN), CL_TRUE, mMaxPoints *sizeof(uint[NUM_GENES]), sizeof(int), bufI(&m_Fluid, FEPIGEN), 0, NULL, NULL), "CountingSortFullCL8", "clEnqueueReadBuffer", "FGRIDCNT", mbDebug);
        SaveUintArray_2D( bufI(&m_Fluid, FEPIGEN), mMaxPoints, NUM_GENES, "CountingSortFullCL__m_FluidTemp.bufI(FEPIGEN)3.csv" );

        //clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FEPIGEN), gpuVar(&m_Fluid, FEPIGEN),	/*mMaxPoints*/mNumPoints *sizeof(uint[NUM_GENES]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
        clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FEPIGEN), CL_TRUE, mNumPoints *sizeof(uint[NUM_GENES]), sizeof(int), bufI(&m_Fluid, FEPIGEN), 0, NULL, NULL), "PrefixSumChangesCL", "clEnqueueReadBuffer", "FGRIDCNT", mbDebug);
        SaveUintArray_2D( bufI(&m_Fluid, FEPIGEN), mMaxPoints, NUM_GENES, "CountingSortFullCL__bufI(&m_Fluid, FEPIGEN)3.csv" );

        //clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FGRIDCNT), gpuVar(&m_Fluid, FGRIDCNT),	sizeof(uint[m_GridTotal]) ), "CountingSortFullCL9", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
        clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FGRIDCNT), CL_TRUE, sizeof(uint[m_GridTotal]), sizeof(int), bufI(&m_Fluid, FGRIDCNT), 0, NULL, NULL), "PrefixSumChangesCL", "clEnqueueReadBuffer", "FGRIDCNT", mbDebug);
        SaveUintArray( bufI(&m_Fluid, FGRIDCNT), m_GridTotal, "CountingSortFullCL__bufI(&m_Fluid, FGRIDCNT).csv" );

        //clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FGRIDOFF), gpuVar(&m_Fluid, FGRIDOFF),	sizeof(uint[m_GridTotal]) ), "CountingSortFullCL10", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);
        clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FGRIDOFF), CL_TRUE, sizeof(uint[m_GridTotal] ), sizeof(int), bufI(&m_Fluid, FGRIDOFF), 0, NULL, NULL), "PrefixSumChangesCL", "clEnqueueReadBuffer", "FGRIDCNT", mbDebug);
        SaveUintArray( bufI(&m_Fluid, FGRIDOFF), m_GridTotal, "CountingSortFullCL__bufI(&m_Fluid, FGRIDOFF).csv" );

       // uint fDenseList2[100000];
       // cl_device_idptr*  _list2pointer = (cl_device_idptr*) &bufC(&m_Fluid, FDENSE_LISTS)[2 * sizeof(cl_device_idptr)];
       // clCheck( cuMemcpyDtoH ( fDenseList2, *_list2pointer,	sizeof(uint[ bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[2] ])/*sizeof(uint[2000])*/ ), "CountingSortFullCL11", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
       // SaveUintArray( fDenseList2, bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[2], "CountingSortFullCL__m_Fluid.bufII(FDENSE_LISTS)[2].csv" );
    }
}

void FluidSystem::TransferFromCL() {
    //std::cout<<"\nTransferFromCL () \n"<<std::flush;
    // Return particle buffers
    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FPOS), CL_TRUE, 0,             mMaxPoints * sizeof(float) * 3, bufC(&m_Fluid, FPOS), 0, NULL, NULL),      "TransferFromCL", "clEnqueueReadBuffer", "FPOS", mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FVEL), CL_TRUE, 0,             mMaxPoints * sizeof(float) * 3, bufC(&m_Fluid, FVEL), 0, NULL, NULL),      "TransferFromCL", "clEnqueueReadBuffer", "FVEL", mbDebug);

    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_Fluid, FVEVAL), CL_TRUE, 0,           mMaxPoints * sizeof(float) * 3, bufC(&m_Fluid, FVEVAL), 0, NULL, NULL),     "TransferFromCL", "clEnqueueReadBuffer", "FVEVAL", mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_FluidTemp, FFORCE), CL_TRUE, 0,       mMaxPoints * sizeof(float) * 3, bufC(&m_FluidTemp, FFORCE), 0, NULL, NULL), "TransferFromCL", "clEnqueueReadBuffer", "FFORCE", mbDebug);
    //NB PhysicalSort zeros gpuVar(&m_FluidTemp, FFORCE)
    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_FluidTemp, FPRESS), CL_TRUE, 0,       mMaxPoints * sizeof(float),     bufC(&m_FluidTemp, FPRESS), 0, NULL, NULL),     "TransferFromCL", "clEnqueueReadBuffer", "FPRESS", mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue, gpuVar(&m_FluidTemp, FDENSITY), CL_TRUE, 0,     mMaxPoints * sizeof(float),     bufC(&m_FluidTemp, FDENSITY), 0, NULL,NULL),    "TransferFromCL", "clEnqueueReadBuffer", "FDENSITY", mbDebug);

    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FAGE), CL_TRUE ,0 ,          mMaxPoints*sizeof(uint) ,       bufC(&m_FluidTemp,FAGE) ,0 ,NULL ,NULL) ,         "TransferFromCL" ,"clEnqueueReadBuffer" ,"FAGE" ,mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FCLR), CL_TRUE ,0 ,          mMaxPoints*sizeof(uint) ,       bufC(&m_FluidTemp,FCLR) ,0 ,NULL ,NULL) ,         "TransferFromCL" ,"clEnqueueReadBuffer" ,"FCLR" ,mbDebug);

    // add extra data for morphogenesis
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FELASTIDX), CL_TRUE ,0 ,     mMaxPoints*sizeof(uint[BOND_DATA]) ,                bufC(&m_FluidTemp,FELASTIDX) ,0 ,NULL ,NULL) ,"TransferFromCL" ,"clEnqueueReadBuffer" ,"FELASTIDX" ,mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FPARTICLEIDX), CL_TRUE ,0 ,  mMaxPoints*sizeof(uint[BONDS_PER_PARTICLE *2]),     bufC(&m_FluidTemp,FPARTICLEIDX) ,0 ,NULL ,NULL) ,"TransferFromCL" ,"clEnqueueReadBuffer" ,"FPARTICLEIDX" ,mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FPARTICLE_ID), CL_TRUE ,0 ,  mMaxPoints*sizeof(uint) ,                           bufC(&m_FluidTemp,FPARTICLE_ID) ,0 ,NULL,NULL) ,         "TransferFromCL" ,"clEnqueueReadBuffer" ,"FPARTICLE_ID" ,mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FMASS_RADIUS), CL_TRUE ,0 ,  mMaxPoints*sizeof(uint) ,                           bufC(&m_FluidTemp,FMASS_RADIUS) ,0 ,NULL,NULL) ,         "TransferFromCL" ,"clEnqueueReadBuffer" ,"FMASS_RADIUS" ,mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FNERVEIDX), CL_TRUE ,0 ,     mMaxPoints*sizeof(uint) ,                           bufC(&m_FluidTemp,FNERVEIDX) ,0 ,NULL,NULL) ,            "TransferFromCL" ,"clEnqueueReadBuffer" ,"FNERVEIDX" ,mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_Fluid, FCONC), CL_TRUE ,0 ,              mMaxPoints*sizeof(float[NUM_TF]) ,                bufC(&m_Fluid,FCONC) ,0,NULL,NULL),             "TransferFromCL","clEnqueueReadBuffer","FCONC",mbDebug);
    clCheck( clEnqueueReadBuffer(m_queue,gpuVar(&m_FluidTemp,FEPIGEN), CL_TRUE ,0,        mMaxPoints*sizeof(uint[NUM_GENES]),                 bufC(&m_FluidTemp,FEPIGEN),0,NULL,NULL),       "TransferFromCL","clEnqueueReadBuffer","FEPIGEN",mbDebug);

}
//
// void FluidSystem::PrefixSumChangesCL ( int zero_offsets ){
//     // Prefix Sum - determine grid offsets
//     int blockSize = SCAN_BLOCKSIZE << 1;                // NB 1024 = 512 << 1.  NB SCAN_BLOCKSIZE is the number of threads per block
//     int numElem1 = m_GridTotal;                         // tot num bins, computed in SetupGrid()
//     int numElem2 = int ( numElem1 / blockSize ) + 1;    // num sheets of bins? NB not spatial, but just dividing the linear array of bins, by a factor of 512*2
//     int numElem3 = int ( numElem2 / blockSize ) + 1;    // num rows of bins?
//     int threads = SCAN_BLOCKSIZE;
//     int zon=1;
//     cl_device_idptr array1  ;		// input
//     cl_device_idptr scan1   ;		// output
//     cl_device_idptr array2  = gpuVar(&m_Fluid, FAUXARRAY1);
//     cl_device_idptr scan2   = gpuVar(&m_Fluid, FAUXSCAN1);
//     cl_device_idptr array3  = gpuVar(&m_Fluid, FAUXARRAY2);
//     cl_device_idptr scan3   = gpuVar(&m_Fluid, FAUXSCAN2);
//
//     // Loop to PrefixSum the Dense Lists - NB by doing one change_list at a time, we reuse the FAUX* arrays & scans.
//     // For each change_list, input FGRIDCNT_ACTIVE_GENES[change_list*m_GridTotal], output FGRIDOFF_ACTIVE_GENES[change_list*m_GridTotal]
//     cl_device_idptr array0  = gpuVar(&m_Fluid, FGRIDCNT_CHANGES);
//     cl_device_idptr scan0   = gpuVar(&m_Fluid, FGRIDOFF_CHANGES);
//
//     if(m_debug>3){
//         // debug chk
//         cout<<"\nSaving (FGRIDCNT_CHANGES): (bin,#particles) , numElem1="<<numElem1<<"\t"<<std::flush;
//         clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FGRIDCNT_CHANGES), gpuVar(&m_Fluid, FGRIDCNT_CHANGES),	sizeof(uint[numElem1]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FGRIDCNT_CHANGES", mbDebug); // NUM_CHANGES*
//         //### print to a csv file   AND do the same afterwards for FGRIDOFF_CHANGES ###
//         SaveUintArray( bufI(&m_Fluid, FGRIDCNT_CHANGES), numElem1, "bufI(&m_Fluid, FGRIDCNT_CHANGES).csv" );
//         //
//         cout<<"\nSaving (FGCELL): (particleIdx, cell) , mMaxPoints="<<mMaxPoints<<"\t"<<std::flush;
//         clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FGCELL), gpuVar(&m_Fluid, FGCELL),	sizeof(uint[mMaxPoints]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FGCELL", mbDebug);
//         SaveUintArray( bufI(&m_Fluid, FGCELL), mMaxPoints, "bufI(&m_Fluid, FGCELL).csv" );
//         //
//         //   clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FDENSE_LISTS), gpuVar(&m_Fluid, FDENSE_LISTS),	sizeof(uint[mMaxPoints]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FDENSE_LISTS", mbDebug);
//         //   SaveUintArray( bufI(&m_Fluid, FDENSE_LISTS), numElem1, "bufI(&m_Fluid, FDENSE_LISTS).csv" );
//     }
//     clFinish ();
//
//     for(int change_list=0; change_list<NUM_CHANGES; change_list++){
//         array1  = array0 + change_list*numElem1*sizeof(int); //gpuVar(&m_Fluid, FGRIDCNT_ACTIVE_GENES);//[change_list*numElem1]   ;      // cl_device_idptr to change_list within gpuVar(&m_Fluid, FGRIDCNT_CHANGES), for start of prefix-sum.
//         scan1   = scan0 + change_list*numElem1*sizeof(int);
//         clCheck ( cuMemsetD8 ( scan1,  0,	numElem1*sizeof(int) ), "PrefixSumChangesCL", "cuMemsetD8", "FGRIDCNT", mbDebug );
//         clCheck ( cuMemsetD8 ( array2, 0,	numElem2*sizeof(int) ), "PrefixSumChangesCL", "cuMemsetD8", "FGRIDCNT", mbDebug );
//         clCheck ( cuMemsetD8 ( scan2,  0,	numElem2*sizeof(int) ), "PrefixSumChangesCL", "cuMemsetD8", "FGRIDCNT", mbDebug );
//         clCheck ( cuMemsetD8 ( array3, 0,	numElem3*sizeof(int) ), "PrefixSumChangesCL", "cuMemsetD8", "FGRIDCNT", mbDebug );
//         clCheck ( cuMemsetD8 ( scan3,  0,	numElem3*sizeof(int) ), "PrefixSumChangesCL", "cuMemsetD8", "FGRIDCNT", mbDebug );
//
//         void* argsA[5] = {&array1, &scan1, &array2, &numElem1, &zero_offsets };     // sum array1. output -> scan1, array2.         i.e. FGRIDCNT -> FGRIDOFF, FAUXARRAY1
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_FPREFIXSUM], numElem2, 1, 1, threads, 1, 1, 0, NULL, argsA, NULL ),
//                   "PrefixSumChangesCL", "cuLaunch", "FUNC_PREFIXSUM", mbDebug);
//         void* argsB[5] = { &array2, &scan2, &array3, &numElem2, &zon };             // sum array2. output -> scan2, array3.         i.e. FAUXARRAY1 -> FAUXSCAN1, FAUXARRAY2
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_FPREFIXSUM], numElem3, 1, 1, threads, 1, 1, 0, NULL, argsB, NULL ), "PrefixSumChangesCL", "cuLaunch", "FUNC_PREFIXSUM", mbDebug);
//         if ( numElem3 > 1 ) {
//             cl_device_idptr nptr = {0};
//             void* argsC[5] = { &array3, &scan3, &nptr, &numElem3, &zon };	        // sum array3. output -> scan3                  i.e. FAUXARRAY2 -> FAUXSCAN2, &nptr
//             clCheck ( cuLaunchKernel ( m_Kern[FUNC_FPREFIXSUM], 1, 1, 1, threads, 1, 1, 0, NULL, argsC, NULL ), "PrefixSumChangesCL", "cuLaunch", "FUNC_PREFIXFIXUP", mbDebug);
//             void* argsD[3] = { &scan2, &scan3, &numElem2 };	                        // merge scan3 into scan2. output -> scan2      i.e. FAUXSCAN2, FAUXSCAN1 -> FAUXSCAN1
//             clCheck ( cuLaunchKernel ( m_Kern[FUNC_FPREFIXFIXUP], numElem3, 1, 1, threads, 1, 1, 0, NULL, argsD, NULL ), "PrefixSumChangesCL", "cuLaunch", "FUNC_PREFIXFIXUP", mbDebug);
//         }
//         void* argsE[3] = { &scan1, &scan2, &numElem1 };		                        // merge scan2 into scan1. output -> scan1      i.e. FAUXSCAN1, FGRIDOFF -> FGRIDOFF
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_FPREFIXFIXUP], numElem2, 1, 1, threads, 1, 1, 0, NULL, argsE, NULL ), "PrefixSumChangesCL", "cuLaunch", "FUNC_PREFIXFIXUP", mbDebug);
//     }
//
//     int num_lists = NUM_CHANGES, length = FDENSE_LIST_LENGTHS_CHANGES, fgridcnt = FGRIDCNT_CHANGES, fgridoff = FGRIDOFF_CHANGES;
//     void* argsF[4] = {&num_lists, &length,&fgridcnt,&fgridoff};
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_TALLYLISTS], NUM_CHANGES, 1, 1, NUM_CHANGES, 1, 1, 0, NULL, argsF, NULL ), "PrefixSumChangesCL", "cuLaunch", "FUNC_TALLYLISTS", mbDebug); //256 threads launched
//     clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES), gpuVar(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES),	sizeof(uint[NUM_CHANGES]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FDENSE_LIST_LENGTHS_CHANGES", mbDebug);
//
//                                                                                                                     // If active particles for change_list > existing buff, then enlarge buff.
//     for(int change_list=0;change_list<NUM_CHANGES;change_list++){                                                   // Note this calculation could be done by a kernel,
//         uint * densebuff_len = bufI(&m_Fluid, FDENSE_BUF_LENGTHS_CHANGES);                                            // and only bufI(&m_Fluid, FDENSE_LIST_LENGTHS); copied to host.
//         uint * denselist_len = bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES);                                           // For each change_list allocate intial buffer,
//         //if (m_FParams.debug>1)printf("\nPrefixSumChangesCL: change_list=%u,  densebuff_len[change_list]=%u, denselist_len[change_list]=%u ,",change_list, densebuff_len[change_list], denselist_len[change_list] );
//         if (denselist_len[change_list] > densebuff_len[change_list]) {                                              // write pointer and size to FDENSE_LISTS and FDENSE_LIST_LENGTHS
//             while(denselist_len[change_list] >  densebuff_len[change_list])   densebuff_len[change_list] *=4;       // bufI(&m_Fluid, FDENSE_BUF_LENGTHS)[i].
//                                                                                                                     // NB Need 2*densebuff_len[change_list] for particle & bond
//             if (m_FParams.debug>1)printf("\nPrefixSumChangesCL: ## enlarging buffer## change_list=%u,  densebuff_len[change_list]=%u, denselist_len[change_list]=%u ,",change_list, densebuff_len[change_list], denselist_len[change_list] );
//             AllocateBufferDenseLists( change_list, sizeof(uint), 2*densebuff_len[change_list], FDENSE_LISTS_CHANGES );// NB frees previous buffer &=> clears data
//         }                                                                                                           // NB buf[2][list_length] holding : particleIdx, bondIdx
//     }
//     clCheck( cuMemcpyHtoD ( gpuVar(&m_Fluid, FDENSE_BUF_LENGTHS_CHANGES), bufC(&m_Fluid, FDENSE_BUF_LENGTHS_CHANGES),	sizeof(uint[NUM_CHANGES]) ), "PrefixSumChangesCL", "cuMemcpyHtoD", "FDENSE_BUF_LENGTHS_CHANGES", mbDebug);
//     cuMemcpyHtoD(gpuVar(&m_Fluid, FDENSE_LISTS_CHANGES), bufC(&m_Fluid, FDENSE_LISTS_CHANGES),  NUM_CHANGES * sizeof(cl_device_idptr)  );                      // update pointers to lists on device
//
//     if (m_FParams.debug>1) {
//         std::cout << "\nChk: PrefixSumChangesCL 4"<<std::flush;
//         for(int change_list=0;change_list<NUM_CHANGES;change_list++){
//             std::cout<<"\nPrefixSumChangesCL: change list_length["<<change_list<<"]="<<bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES)[change_list]<<"\t"<<std::flush;
//         }
//     }
// }
//
// void FluidSystem::CountingSortFullCL ( Vector3DF* ppos ){
//     if (m_FParams.debug>1) std::cout << "\nCountingSortFullCL()1: mMaxPoints="<<mMaxPoints<<", mNumPoints="<<mNumPoints<<",\tmActivePoints="<<mActivePoints<<".\n"<<std::flush;
//     // get number of active particles & set short lists for later kernels
//     int grid_ScanMax = (m_FParams.gridScanMax.y * m_FParams.gridRes.z + m_FParams.gridScanMax.z) * m_FParams.gridRes.x + m_FParams.gridScanMax.x;
//
//     clCheck( cuMemcpyDtoH ( &mNumPoints,  gpuVar(&m_Fluid, FGRIDOFF)+(m_GridTotal-1/*grid_ScanMax+1*/)*sizeof(int), sizeof(int) ), "CountingSortFullCL1", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);
//
//     clCheck( cuMemcpyDtoH ( &mActivePoints,  gpuVar(&m_Fluid, FGRIDOFF)+(grid_ScanMax/*-1*/)*sizeof(int), sizeof(int) ), "CountingSortFullCL2", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);
//     /*
//     int totalPoints = 0;
//     clCheck( cuMemcpyDtoH ( &totalPoints,  gpuVar(&m_Fluid, FGRIDOFF)+(m_GridTotal)*sizeof(int), sizeof(int) ), "CountingSortFullCL3", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);
//     std::cout<<"\nCountingSortFullCL(): totalPoints="<<totalPoints<<std::flush;
//     */
//     m_FParams.pnumActive = mActivePoints;                                     // TODO eliminate duplication of information & variables between fluid.h and fluid_system.h
//     clCheck ( cuMemcpyHtoD ( clFParams,	&m_FParams, sizeof(FParams) ), "CountingSortFullCL3", "cuMemcpyHtoD", "clFParams", mbDebug); // seems the safest way to update fparam.pnumActive on device.
//
//     if (m_FParams.debug>1) std::cout<<"\nCountingSortFullCL()2: mMaxPoints="<<mMaxPoints<<" mNumPoints="<<mNumPoints<<",\tmActivePoints="<<mActivePoints<<",  m_GridTotal="<<m_GridTotal<<", grid_ScanMax="<<grid_ScanMax<<"\n"<<std::flush;
//
//     // Transfer particle data to temp buffers
//     //  (gpu-to-gpu copy, no sync needed)
//     //TransferToTempCL ( FPOS,		mMaxPoints *sizeof(Vector3DF) );    // NB if some points have been removed, then the existing list is no longer dense,
//     //TransferToTempCL ( FVEL,		mMaxPoints *sizeof(Vector3DF) );    // hence must use mMaxPoints, not mNumPoints
//     //TransferToTempCL ( FVEVAL,	mMaxPoints *sizeof(Vector3DF) );    // { Could potentially use (old_mNumPoints + mNewPoints) instead of mMaxPoints}
//     TransferToTempCL ( FFORCE,	mMaxPoints *sizeof(Vector3DF) );    // NB buffers are declared and initialized on mMaxPoints.
//     TransferToTempCL ( FPRESS,	mMaxPoints *sizeof(float) );
//     TransferToTempCL ( FDENSITY,	mMaxPoints *sizeof(float) );
//     TransferToTempCL ( FCLR,		mMaxPoints *sizeof(uint) );
//     TransferToTempCL ( FAGE,		mMaxPoints *sizeof(uint) );
//     TransferToTempCL ( FGCELL,	mMaxPoints *sizeof(uint) );
//     TransferToTempCL ( FGNDX,		mMaxPoints *sizeof(uint) );
//
//     // extra data for morphogenesis
//     TransferToTempCL ( FELASTIDX,		mMaxPoints *sizeof(uint[BOND_DATA]) );
//     TransferToTempCL ( FPARTICLEIDX,	mMaxPoints *sizeof(uint[BONDS_PER_PARTICLE *2]) );
//     TransferToTempCL ( FPARTICLE_ID,	mMaxPoints *sizeof(uint) );
//     TransferToTempCL ( FMASS_RADIUS,	mMaxPoints *sizeof(uint) );
//     TransferToTempCL ( FNERVEIDX,		mMaxPoints *sizeof(uint) );
//     TransferToTempCL ( FCONC,		    mMaxPoints *sizeof(float[NUM_TF]) );
//     TransferToTempCL ( FEPIGEN,	    mMaxPoints *sizeof(uint[NUM_GENES]) );
//
//     // debug chk
//     //clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FEPIGEN), gpuVar(&m_FluidTemp, FEPIGEN),	mMaxPoints *sizeof(uint[NUM_GENES]) ), "CountingSortFullCL4", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
//     //SaveUintArray_2D( bufI(&m_Fluid, FEPIGEN), mMaxPoints, NUM_GENES, "CountingSortFullCL__m_FluidTemp.bufI(FEPIGEN)2.csv" );
//
//     // reset bonds and forces in fbuf FELASTIDX, FPARTICLEIDX and FFORCE, required to prevent interference between time steps,
//     // because these are not necessarily overwritten by the FUNC_COUNTING_SORT kernel.
//     clFinish ();    // needed to prevent colision with previous operations
//
//     float max_pos = max(max(m_Vec[PVOLMAX].x, m_Vec[PVOLMAX].y), m_Vec[PVOLMAX].z);
//     uint * uint_max_pos = (uint*)&max_pos;
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FPOS), *uint_max_pos, mMaxPoints * 3 ),  "CountingSortFullCL", "cuMemsetD32", "FELASTIDX",   mbDebug);
//
//     //cout<<"\nCountingSortFullCL: m_Vec[PVOLMAX]=("<<m_Vec[PVOLMAX].x<<", "<<m_Vec[PVOLMAX].y<<", "<<m_Vec[PVOLMAX].z<<"),  max_pos = "<< max_pos <<std::flush;
//     // NB resetting  gpuVar(&m_Fluid, FPOS)  ensures no zombie particles. ?hopefully?
//
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FELASTIDX),    UINT_MAX,  mMaxPoints * BOND_DATA              ),  "CountingSortFullCL", "cuMemsetD32", "FELASTIDX",    mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FPARTICLEIDX), UINT_MAX,  mMaxPoints * BONDS_PER_PARTICLE *2  ),  "CountingSortFullCL", "cuMemsetD32", "FPARTICLEIDX", mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FPARTICLE_ID), UINT_MAX,  mMaxPoints                          ),  "CountingSortFullCL", "cuMemsetD32", "FPARTICLEIDX", mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FFORCE),      (uint)0.0,  mMaxPoints * 3 /* ie num elements */),  "CountingSortFullCL", "cuMemsetD32", "FFORCE",       mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FCONC),             0.0,  mMaxPoints * NUM_TF                 ),  "CountingSortFullCL", "cuMemsetD32", "FCONC",        mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FEPIGEN),     (uint)0.0,  mMaxPoints * NUM_GENES              ),  "CountingSortFullCL", "cuMemsetD32", "FEPIGEN",      mbDebug);
//     clFinish ();    // needed to prevent colision with previous operations
//
//     // Reset grid cell IDs
//     // clCheck(cuMemsetD32(gpuVar(&m_Fluid, FGCELL), GRID_UNDEF, numPoints ), "cuMemsetD32(Sort)");
//     void* args[1] = { &mMaxPoints };
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COUNTING_SORT], m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL),
//               "CountingSortFullCL5", "cuLaunch", "FUNC_COUNTING_SORT", mbDebug );
//
//     // Having sorted the particle data, we can start using a shortened list of particles.
//     // NB have to reset to long list at start of time step.
//     computeNumBlocks ( m_FParams.pnumActive, m_FParams.threadsPerBlock, m_FParams.numBlocks, m_FParams.numThreads);				// particles
//
//     if (m_FParams.debug>1) std::cout<<"\n CountingSortFullCL : FUNC_COUNT_SORT_LISTS\n"<<std::flush;
//     // countingSortDenseLists ( int pnum ) // NB launch on bins not particles.
//     int blockSize = SCAN_BLOCKSIZE/2 << 1;
//     int numElem1 = m_GridTotal;
//     int numElem2 = 2*  int( numElem1 / blockSize ) + 1;
//     int threads = SCAN_BLOCKSIZE/2;
//     clFinish ();
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COUNT_SORT_LISTS], /*m_FParams.numBlocks*/ numElem2, 1, 1, /*m_FParams.numThreads/2*/ threads , 1, 1, 0, NULL, args, NULL),
//               "CountingSortFullCL7", "cuLaunch", "FUNC_COUNT_SORT_LISTS", mbDebug );                                   // NB threads/2 required on GTX970m
//     clFinish ();
//
//     if(m_FParams.debug>3){//debug chk
//         std::cout<<"\n### Saving UintArray .csv files."<<std::flush;
//
//         clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FEPIGEN), gpuVar(&m_FluidTemp, FEPIGEN),	mMaxPoints *sizeof(uint[NUM_GENES]) ), "CountingSortFullCL8", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
//         SaveUintArray_2D( bufI(&m_Fluid, FEPIGEN), mMaxPoints, NUM_GENES, "CountingSortFullCL__m_FluidTemp.bufI(FEPIGEN)3.csv" );
//
//         clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FEPIGEN), gpuVar(&m_Fluid, FEPIGEN),	/*mMaxPoints*/mNumPoints *sizeof(uint[NUM_GENES]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
//         SaveUintArray_2D( bufI(&m_Fluid, FEPIGEN), mMaxPoints, NUM_GENES, "CountingSortFullCL__bufI(&m_Fluid, FEPIGEN)3.csv" );
//
//         clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FGRIDCNT), gpuVar(&m_Fluid, FGRIDCNT),	sizeof(uint[m_GridTotal]) ), "CountingSortFullCL9", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
//         SaveUintArray( bufI(&m_Fluid, FGRIDCNT), m_GridTotal, "CountingSortFullCL__bufI(&m_Fluid, FGRIDCNT).csv" );
//
//         clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FGRIDOFF), gpuVar(&m_Fluid, FGRIDOFF),	sizeof(uint[m_GridTotal]) ), "CountingSortFullCL10", "cuMemcpyDtoH", "FGRIDOFF", mbDebug);
//         SaveUintArray( bufI(&m_Fluid, FGRIDOFF), m_GridTotal, "CountingSortFullCL__bufI(&m_Fluid, FGRIDOFF).csv" );
//
//        // uint fDenseList2[100000];
//        // cl_device_idptr*  _list2pointer = (cl_device_idptr*) &bufC(&m_Fluid, FDENSE_LISTS)[2 * sizeof(cl_device_idptr)];
//        // clCheck( cuMemcpyDtoH ( fDenseList2, *_list2pointer,	sizeof(uint[ bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[2] ])/*sizeof(uint[2000])*/ ), "CountingSortFullCL11", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
//        // SaveUintArray( fDenseList2, bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[2], "CountingSortFullCL__m_Fluid.bufII(FDENSE_LISTS)[2].csv" );
//     }
// }
//
// void FluidSystem::CountingSortChangesCL ( ){
//     //std::cout<<"\n\n#### CountingSortChangesCL ( ):   m_FParams.debug = "<< m_FParams.debug <<"\n";
//     if (m_FParams.debug>1) {std::cout<<"\n\n#### CountingSortChangesCL ( )"<<std::flush;}
//     /* ////////
//     clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES), gpuVar(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES),	sizeof(uint[NUM_CHANGES]) ), "PrefixSumCellsCL", "cuMemcpyDtoH", "FDENSE_LIST_LENGTHS_CHANGES", mbDebug);
//
//                                                                                                                     // If active particles for change_list > existing buff, then enlarge buff.
//     for(int change_list=0;change_list<NUM_CHANGES;change_list++){                                                   // Note this calculation could be done by a kernel,
//         uint * densebuff_len = bufI(&m_Fluid, FDENSE_BUF_LENGTHS_CHANGES);                                            // and only bufI(&m_Fluid, FDENSE_LIST_LENGTHS); copied to host.
//         uint * denselist_len = bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES);                                           // For each change_list allocate intial buffer,
//         if (m_FParams.debug>1)printf("\nCountingSortChangesCL1: change_list=%u,  densebuff_len[change_list]=%u, denselist_len[change_list]=%u ,",change_list, densebuff_len[change_list], denselist_len[change_list] );
//     }
//     *//////////
//     int blockSize = SCAN_BLOCKSIZE/2 << 1;
//     int numElem1 = m_GridTotal;
//     int numElem2 = 2* int( numElem1 / blockSize ) + 1;
//     int threads = SCAN_BLOCKSIZE/2;
//     void* args[1] = { &mActivePoints };
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COUNTING_SORT_CHANGES], numElem2, 1, 1, threads , 1, 1, 0, NULL, args, NULL),
//               "CountingSortChangesCL", "cuLaunch", "FUNC_COUNTING_SORT_CHANGES", mbDebug );
//      /////////
//     clCheck(clFinish(), "CountingSortChangesCL()", "clFinish", "After FUNC_COUNTING_SORT_CHANGES", mbDebug);
//
//     clCheck( cuMemcpyDtoH ( bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES), gpuVar(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES),	sizeof(uint[NUM_CHANGES]) ), "PrefixSumCellsCL", "cuMemcpyDtoH", "FDENSE_LIST_LENGTHS_CHANGES", mbDebug);
//                                                                                                                     // If active particles for change_list > existing buff, then enlarge buff.
//     for(int change_list=0;change_list<NUM_CHANGES;change_list++){                                                   // Note this calculation could be done by a kernel,
//         uint * densebuff_len = bufI(&m_Fluid, FDENSE_BUF_LENGTHS_CHANGES);                                            // and only bufI(&m_Fluid, FDENSE_LIST_LENGTHS); copied to host.
//         uint * denselist_len = bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES);                                           // For each change_list allocate intial buffer,
//         if (m_FParams.debug>1)printf("\nCountingSortChangesCL2: change_list=%u,  densebuff_len[change_list]=%u, denselist_len[change_list]=%u ,\t\t threads=%u, numElem2=%u,  m_GridTotal=%u \t",
//                change_list, densebuff_len[change_list], denselist_len[change_list], threads, numElem2,  m_GridTotal );
//         clFinish ();
//         if(m_FParams.debug>0){
//             uint fDenseList2[1000000] = {UINT_MAX};//TODO make this array size safe!  NB 10* num particles.
//             cl_device_idptr*  _list2pointer = (cl_device_idptr*) &bufC(&m_Fluid, FDENSE_LISTS_CHANGES)[change_list*sizeof(cl_device_idptr)];
//                                                                                                                 // Get device pointer to FDENSE_LISTS_CHANGES[change_list].
//             clCheck( cuMemcpyDtoH ( fDenseList2, *_list2pointer,	2*sizeof(uint[densebuff_len[change_list]]) ), "PrefixSumChangesCL", "cuMemcpyDtoH", "FGRIDCNT", mbDebug);
//             char filename[256];
//             sprintf(filename, "CountingSortChangesCL__m_Fluid.bufII(FDENSE_LISTS_CHANGES)[%u].csv", change_list);
//             SaveUintArray_2Columns( fDenseList2, denselist_len[change_list], densebuff_len[change_list], filename );
//             ///
//             printf("\n\n*_list2pointer=%llu",*_list2pointer);
//
//         }
//     }
// }
//
// void FluidSystem::InitializeBondsCL (){
//     if (m_FParams.debug>1)cout << "\n\nInitializeBondsCL ()\n"<<std::flush;
//     uint gene           = 1;                                                            // solid  (has springs)
//     uint list_length    = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene];
//     void* args[3]       = { &m_FParams.pnumActive, &list_length, &gene};                //initialize_bonds (int ActivePoints, uint list_length, uint gene)
//     int numBlocks, numThreads;
//     computeNumBlocks (list_length, m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//     if (m_FParams.debug>1)cout << "\nInitializeBondsCL (): list_length="<<list_length<<", m_FParams.threadsPerBlock="<<m_FParams.threadsPerBlock<<", numBlocks="<<numBlocks<<", numThreads="<<numThreads<<" \t args{m_FParams.pnumActive="<<m_FParams.pnumActive<<", list_length="<<list_length<<", gene="<<gene<<"}"<<std::flush;
//
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_INITIALIZE_BONDS],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "ComputePressureCL", "cuLaunch", "FUNC_COMPUTE_PRESS", mbDebug);
// }
//
// void FluidSystem::ComputePressureCL (){
//     void* args[1] = { &mActivePoints };
//     //cout<<"\nComputePressureCL: mActivePoints="<<mActivePoints<<std::flush;
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COMPUTE_PRESS],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "ComputePressureCL", "cuLaunch", "FUNC_COMPUTE_PRESS", mbDebug);
// }
//
// void FluidSystem::ComputeDiffusionCL(){
//     //if (m_FParams.debug>1) std::cout << "\n\nRunning ComputeDiffusionCL()" << std::endl;
//     void* args[1] = { &mActivePoints };
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COMPUTE_DIFFUSION],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "ComputeDiffusionCL", "cuLaunch", "FUNC_COMPUTE_DIFFUSION", mbDebug);
// }
//
// void FluidSystem::ComputeForceCL (){
//     //if (m_FParams.debug>1)printf("\n\nFluidSystem::ComputeForceCL (),  m_FParams.freeze=%s",(m_FParams.freeze==true) ? "true" : "false");
//     void* args[3] = { &m_FParams.pnumActive ,  &m_FParams.freeze, &m_FParams.frame};
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COMPUTE_FORCE],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "ComputeForceCL", "cuLaunch", "FUNC_COMPUTE_FORCE", mbDebug);
// }
//
// void FluidSystem::ComputeGenesCL (){  // for each gene, call a kernel wih the dese list for that gene
//     // NB Must zero ftemp.bufI(FEPIGEN) and ftemp.bufI(FCONC) before calling kernel. ftemp is used to collect changes before FUNC_TALLY_GENE_ACTION.
//     clCheck ( cuMemsetD8 ( gpuVar(&m_FluidTemp, FCONC),   0,	m_FParams.szPnts *sizeof(float[NUM_TF])   ), "ComputeGenesCL", "cuMemsetD8", "gpuVar(&m_FluidTemp, FCONC)",   mbDebug );
//     clCheck ( cuMemsetD8 ( gpuVar(&m_FluidTemp, FEPIGEN), 0,	m_FParams.szPnts *sizeof(uint[NUM_GENES]) ), "ComputeGenesCL", "cuMemsetD8", "gpuVar(&m_FluidTemp, FEPIGEN)", mbDebug );
//     for (int gene=0;gene<NUM_GENES;gene++) {
//         uint list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene];
//         void* args[3] = { &m_FParams.pnumActive, &gene, &list_length };
//         int numBlocks, numThreads;
//         computeNumBlocks ( list_length , m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//         if (m_FParams.debug>1) std::cout<<"\nComputeGenesCL (): gene ="<<gene<<", list_length="<<list_length<<", m_FParams.threadsPerBlock="<<m_FParams.threadsPerBlock<<", numBlocks="<<numBlocks<<",  numThreads="<<numThreads<<". args={mNumPoints="<<mNumPoints<<", list_length="<<list_length<<", gene ="<<gene<<"}"<<std::flush;
//
//         if( numBlocks>0 && numThreads>0){
//             //std::cout<<"\nCalling m_Kern[FUNC_COMPUTE_GENE_ACTION], list_length="<<list_length<<", numBlocks="<<numBlocks<<", numThreads="<<numThreads<<"\n"<<std::flush;
//             clCheck ( cuLaunchKernel ( m_Kern[FUNC_COMPUTE_GENE_ACTION],  numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL), "ComputeGenesCL", "cuLaunch", "FUNC_COMPUTE_GENE_ACTION", mbDebug);
//         }
//     }
//     clCheck(clFinish(), "ComputeGenesCL", "clFinish", "After FUNC_COMPUTE_GENE_ACTION & before FUNC_TALLY_GENE_ACTION", mbDebug);
//     for (int gene=0;gene<NUM_GENES;gene++) {
//         uint list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene];
//         void* args[3] = { &m_FParams.pnumActive, &gene, &list_length };
//         int numBlocks, numThreads;
//         computeNumBlocks ( list_length , m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//         if( numBlocks>0 && numThreads>0){
//             if (m_FParams.debug>1) std::cout<<"\nCalling m_Kern[FUNC_TALLY_GENE_ACTION], gene="<<gene<<", list_length="<<list_length<<", numBlocks="<<numBlocks<<", numThreads="<<numThreads<<"\n"<<std::flush;
//             clCheck ( cuLaunchKernel ( m_Kern[FUNC_TALLY_GENE_ACTION],  numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL), "ComputeGenesCL", "cuLaunch", "FUNC_TALLY_GENE_ACTION", mbDebug);
//         }
//     }
// }
//
// void FluidSystem::AssembleFibresCL (){  //kernel: void assembleMuscleFibres ( int pnum, uint list, uint list_length )
//     if (m_FParams.debug>1)cout << "\n\nAssembleFibresCL ()\n"<<std::flush;
//     uint gene = 7; // muscle
//     uint list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene];
//     void* args[3] = { &m_FParams.pnumActive, &gene, &list_length };
//     int numBlocks, numThreads;
//     computeNumBlocks ( list_length , m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//     /*
//     if( numBlocks>0 && numThreads>0){
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_ASSEMBLE_MUSCLE_FIBRES_OUTGOING],  numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL), "ComputeGenesCL", "cuLaunch", "FUNC_COMPUTE_GENE_ACTION", mbDebug);
//     }
//     */
//
//     clCheck(clFinish(), "Run", "clFinish", "In AssembleFibresCL, after OUTGOING", mbDebug);
//
//     /*
//     if( numBlocks>0 && numThreads>0){
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_ASSEMBLE_MUSCLE_FIBRES_INCOMING],  numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL), "ComputeGenesCL", "cuLaunch", "FUNC_COMPUTE_GENE_ACTION", mbDebug);
//     }
//     clCheck(clFinish(), "Run", "clFinish", "In AssembleFibresCL, after OUTGOING", mbDebug);
//     */
//
//
//
//
//     // Kernels:  call by tissue type using dense lists by gene.
//     //assembleMuscleFibres()
//     //assembleFasciaFibres ()
//     if (m_FParams.debug>1) cout << "\nFinished AssembleFibresCL ()\n\n"<<std::flush;
// }
//
// void FluidSystem::ComputeBondChangesCL (uint steps_per_InnerPhysicalLoop){// Given the action of the genes, compute the changes to particle properties & splitting/combining  NB also "inserts changes"
// //  if (m_FParams.debug>1)printf("\n gpuVar(&m_Fluid, FGRIDOFF_CHANGES)=%llu   ,\t gpuVar(&m_Fluid, FGRIDCNT_CHANGES)=%llu   \n",gpuVar(&m_Fluid, FGRIDOFF_CHANGES) , gpuVar(&m_Fluid, FGRIDCNT_CHANGES)   );
//
//     clCheck ( cuMemsetD8 ( gpuVar(&m_Fluid, FGRIDOFF_CHANGES), 0,	m_GridTotal *sizeof(uint[NUM_CHANGES]) ), "ComputeBondChangesCL", "cuMemsetD8", "FGRIDOFF", mbDebug );
//                                             //NB list for all living cells. (non senescent) = FEPIGEN[2]
//     clCheck ( cuMemsetD8 ( gpuVar(&m_Fluid, FGRIDCNT_CHANGES), 0,	m_GridTotal *sizeof(uint[NUM_CHANGES]) ), "ComputeBondChangesCL", "cuMemsetD8", "FGRIDCNT", mbDebug );
//
//     uint list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[2];    // call for dense list of living cells (gene'2'living/telomere (has genes))
//     void* args[3] = { &mActivePoints, &list_length, &steps_per_InnerPhysicalLoop};
//     int numBlocks, numThreads;
//     computeNumBlocks (list_length, m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//     //std::cout<<"\n\nComputeBondChangesCL (): m_FParams.debug = "<<m_FParams.debug<<", (m_FParams.debug>1)="<<(m_FParams.debug>1)<<"\n"<<std::flush;
//
//     if (m_FParams.debug>1) std::cout<<"\n\nComputeBondChangesCL (): list_length="<<list_length<<", m_FParams.threadsPerBlock="<<m_FParams.threadsPerBlock<<", numBlocks="<<numBlocks<<",  numThreads="<<numThreads<<". \t\t args={mActivePoints="<<mActivePoints<<", list_length="<<list_length<<"}\n\n"<<std::flush;
//
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_COMPUTE_BOND_CHANGES],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "computeBondChanges", "cuLaunch", "FUNC_COMPUTE_BOND_CHANGES", mbDebug);
// }
//
// void FluidSystem::ComputeParticleChangesCL (){// Call each for dense list to execute particle changes. NB Must run concurrently without interfering => no clFinish()
//     uint startNewPoints = mActivePoints + 1;
//     if (m_FParams.debug>2)printf("\n");
//     for (int change_list = 0; change_list<NUM_CHANGES;change_list++){
//     //int change_list = 0; // TODO debug, chk one kernel at a time
//         uint list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS_CHANGES)[change_list];  // num blocks and threads by list length
//         //uint list_length = bufI(&m_Fluid, FDENSE_BUF_LENGTHS_CHANGES)[change_list];
//         //if (change_list!=0 && change_list!=1)continue; // only test heal() and lengthenTissue() for now.
//
//         if (m_FParams.debug>2)printf("\n\nComputeParticleChangesCL(): startNewPoints=%u, change_list=%u, list_length=%u, mMaxPoints=%u \t",
//             startNewPoints, change_list, list_length, mMaxPoints);
//
//         if ((change_list >0)&&(startNewPoints + list_length > mMaxPoints)){         // NB heal() does not create new bonds.
//             printf("\n\n### Run out of spare particles. startNewPoints=%u, change_list=%u, list_length=%u, mMaxPoints=%u ###\n",
//             startNewPoints, change_list, list_length, mMaxPoints);
//             list_length = mMaxPoints - startNewPoints;
//             Exit();
//         }//
//
//         void* args[5] = {&mActivePoints, &list_length, &change_list, &startNewPoints, &mMaxPoints};
//         int numThreads, numBlocks;
//
//         //int numThreads = 1;//m_FParams.threadsPerBlock;
//         //int numBlocks  = 1;//iDivUp ( list_length, numThreads );
//
//         computeNumBlocks (list_length, m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//         if (m_FParams.debug>2) std::cout
//             <<"\nComputeParticleChangesCL ():"
//             <<" frame ="                    <<m_FParams.frame
//             <<", mActivePoints="            <<mActivePoints
//             <<", change_list ="             <<change_list
//             <<", list_length="              <<list_length
//             <<", m_FParams.threadsPerBlock="<<m_FParams.threadsPerBlock
//             <<", numBlocks="                <<numBlocks
//             <<", numThreads="               <<numThreads
//             <<". args={mActivePoints="      <<mActivePoints
//             <<", list_length="              <<list_length
//             <<", change_list="              <<change_list
//             <<", startNewPoints="           <<startNewPoints
//             <<"\t"<<std::flush;
//
//         if( (list_length>0) && (numBlocks>0) && (numThreads>0)){
//             if (m_FParams.debug>0) std::cout
//                 <<"\nComputeParticleChangesCL ():"
//                 <<"\tCalling m_Kern[FUNC_HEAL+"             <<change_list
//                 <<"], list_length="                         <<list_length
//                 <<", numBlocks="                            <<numBlocks
//                 <<", numThreads="                           <<numThreads
//                 <<",\t m_FParams.threadsPerBlock="          <<m_FParams.threadsPerBlock
//                 <<", numBlocks*m_FParams.threadsPerBlock="  <<numBlocks*m_FParams.threadsPerBlock
//                 <<"\t"<<std::flush;
//
//             clCheck ( cuLaunchKernel ( m_Kern[FUNC_HEAL+change_list], numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL),
//                   "ComputeParticleChangesCL", "cuLaunch", "FUNC_HEAL+change_list", mbDebug);
//         }
//         clCheck(clFinish(), "ComputeParticleChangesCL", "clFinish", "In ComputeParticleChangesCL", mbDebug);
//                                                                                 // Each thread will pick different new particles from surplus particles.
//         if (change_list==2 || change_list==6) startNewPoints+=  list_length;    // Increment by num new particles used by previous kernels.
//         //if (change_list==1 || change_list==5) startNewPoints+=  list_length*3;  // Increment by 3 particles for muscle.
//         /*
//     0   #define FUNC_HEAL                       23 //heal
//     1   #define FUNC_LENGTHEN_MUSCLE            24 //lengthen_muscle
//     2   #define FUNC_LENGTHEN_TISSUE            25 //lengthen_tissue
//     3   #define FUNC_SHORTEN_MUSCLE             26 //shorten_muscle
//     4   #define FUNC_SHORTEN_TISSUE             27 //shorten_tissue
//
//     5   #define FUNC_STRENGTHEN_MUSCLE          28 //strengthen_muscle
//     6   #define FUNC_STRENGTHEN_TISSUE          29 //strengthen_tissue
//     7   #define FUNC_WEAKEN_MUSCLE              30 //weaken_muscle
//     8   #define FUNC_WEAKEN_TISSUE              31 //weaken_tissue
//          */
//     }
//     if (m_FParams.debug>1) std::cout<<"\nFinished ComputeParticleChangesCL ()\n"<<std::flush;
// }
//
// void FluidSystem::CleanBondsCL (){
//     void* args[3] = { &m_FParams.pnumActive};
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_CLEAN_BONDS],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "CleanBondsCL", "cuLaunch", "FUNC_CLEAN_BONDS", mbDebug);
// }
//
// void FluidSystem::TransferPosVelVeval (){
//     TransferToTempCL ( FPOS,		mMaxPoints *sizeof(Vector3DF) );    // NB if some points have been removed, then the existing list is no longer dense,
//     TransferToTempCL ( FVEL,		mMaxPoints *sizeof(Vector3DF) );    // hence must use mMaxPoints, not mNumPoints
//     TransferToTempCL ( FVEVAL,	mMaxPoints *sizeof(Vector3DF) );
// }
//
// void FluidSystem::TransferPosVelVevalFromTemp (){
//     TransferFromTempCL ( FPOS,	mMaxPoints *sizeof(Vector3DF) );    // NB if some points have been removed, then the existing list is no longer dense,
//     TransferFromTempCL ( FVEL,	mMaxPoints *sizeof(Vector3DF) );    // hence must use mMaxPoints, not mNumPoints
//     TransferFromTempCL ( FVEVAL,	mMaxPoints *sizeof(Vector3DF) );
// }
//
// void FluidSystem::ZeroVelCL (){                                       // Used to remove velocity, kinetic energy and momentum during initialization.
//     clCheck(clFinish(), "Run", "clFinish", "After freeze Run2PhysicalSort ", mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FVEL),   0.0,  mMaxPoints ),  "ZeroVelCL", "cuMemsetD32", "FVEL",        mbDebug);
//     clCheck ( cuMemsetD32 ( gpuVar(&m_Fluid, FVEVAL), 0.0,  mMaxPoints ),  "ZeroVelCL", "cuMemsetD32", "FVEVAL",      mbDebug);
//     clCheck(clFinish(), "Run", "clFinish", "After freeze ZeroVelCL ", mbDebug);
// }
//
// void FluidSystem::AdvanceCL ( float tm, float dt, float ss ){
//     void* args[4] = { &tm, &dt, &ss, &m_FParams.pnumActive };
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_ADVANCE],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "AdvanceCL", "cuLaunch", "FUNC_ADVANCE", mbDebug);
//     //cout<<"\nAdvanceCL: m_FParams.pnumActive="<<m_FParams.pnumActive<<std::flush;
// }
//
// void FluidSystem::SpecialParticlesCL (float tm, float dt, float ss){   // For interaction.Using dense lists for gene 1 & 0.
//     int gene = 12;                                                           // 'externally actuated' particles
//     uint list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene];
//     void* args[5] = {&list_length, &tm, &dt, &ss, &m_FParams.pnumActive};         // void externalActuation (uint list_len,  float time, float dt, float ss, int numPnts )
//     int numBlocks, numThreads;
//     computeNumBlocks ( list_length , m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//     if (m_FParams.debug>1) std::cout<<"\nSpecialParticlesCL:EXTERNAL_ACTUATION: list_length="<<list_length<<" , m_FParams.threadsPerBlock="<< m_FParams.threadsPerBlock <<", numBlocks="<< numBlocks <<", numThreads="<< numThreads <<", args{m_FParams.pnum="<< m_FParams.pnum <<",  gene="<< gene <<", list_length="<< list_length <<"  }  \n"<<std::flush;
//
//     if( numBlocks>0 && numThreads>0){
//         if (m_FParams.debug>1) std::cout<<"\nCalling m_Kern[FUNC_EXTERNAL_ACTUATION], list_length="<<list_length<<", numBlocks="<<numBlocks<<", numThreads="<<numThreads<<"\n"<<std::flush;
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_EXTERNAL_ACTUATION],  numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL), "SpecialParticlesCL", "cuLaunch", "FUNC_EXTERNAL_ACTUATION", mbDebug);
//     }
//     gene =11;                                                                // 'fixed' particles
//     list_length = bufI(&m_Fluid, FDENSE_LIST_LENGTHS)[gene];
//     args[0] = &list_length;                                                 // void fixedParticles (uint list_len, int numPnts )
//     args[1] = &m_FParams.pnum;
//     computeNumBlocks ( list_length , m_FParams.threadsPerBlock, numBlocks, numThreads);
//
//     if (m_FParams.debug>1) std::cout<<"\nSpecialParticlesCL:FIXED: list_length="<<list_length<<" , m_FParams.threadsPerBlock="<< m_FParams.threadsPerBlock <<", numBlocks="<< numBlocks <<", numThreads="<< numThreads <<", args{m_FParams.pnum="<< m_FParams.pnum <<",  gene="<< gene <<", list_length="<< list_length <<"  }  \n"<<std::flush;
//
//     if( numBlocks>0 && numThreads>0){
//         if (m_FParams.debug>1) std::cout<<"\nCalling m_Kern[FUNC_FIXED], list_length="<<list_length<<", numBlocks="<<numBlocks<<", numThreads="<<numThreads<<"\n"<<std::flush;
//         clCheck ( cuLaunchKernel ( m_Kern[FUNC_FIXED],  numBlocks, 1, 1, numThreads, 1, 1, 0, NULL, args, NULL), "SpecialParticlesCL", "cuLaunch", "FUNC_FIXED", mbDebug);
//     }
// }
//
// void FluidSystem::EmitParticlesCL ( float tm, int cnt ){
//     void* args[3] = { &tm, &cnt, &m_FParams.pnum };
//     clCheck ( cuLaunchKernel ( m_Kern[FUNC_EMIT],  m_FParams.numBlocks, 1, 1, m_FParams.numThreads, 1, 1, 0, NULL, args, NULL), "EmitParticlesCL", "cuLaunch", "FUNC_EMIT", mbDebug);
// }
//
