#ifndef __THRESH_CPU__
#define __THRESH_CPU__

#include <cstddef>
#include "wavelet.h"

#define MIN_THRESHOLD_VALUE 1e-16

// Allocates absData on the heap and loads it with abs(data)
// Sorts absData.  Finds the index corresponding to the compRatio, and 
// reads that element to get the threshold. Sets max value and minimum value.
float thresh_cpu(int count, float *data, float compRatio,
		 int *nonzeroCount, float *maxVal, float *minVal,
		 float **saveSortedAbsData = NULL);

float thresh_cpu(CubeFloat *data, float compRatio,
		 int *nonzeroCount, float *maxVal, float *minVal,
		 float **saveSortedAbsData = NULL);

#endif // __THRESH_CPU__
