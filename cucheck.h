#ifndef __CUCHECK_H__
#define __CUCHECK_H__

#include <stdio.h>

#ifdef __CUDACC__

// A macro for checking the error codes of cuda runtime calls
#define CUCHECK(expr) do { \
    cudaError_t err = expr;    \
    if (err != cudaSuccess) { \
      printf("\n  %s:%d: CUDA call failed: %s\n\n", \
	     __FILE__, __LINE__,		    \
	     cudaGetErrorString(err));		    \
      exit(1); }} while(0)

#endif // __CUDACC__

#endif // __CUCHECK_H__

