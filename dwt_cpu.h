#ifndef __DWT_CPU_H__
#define __DWT_CPU_H__

#include "wavelet.h"
#include "cucheck.h"

#define CDF97_ANALYSIS_LOWPASS_FILTER_0  .85269867900940
#define CDF97_ANALYSIS_LOWPASS_FILTER_1  .377402855612650
#define CDF97_ANALYSIS_LOWPASS_FILTER_2 -.110624404418420
#define CDF97_ANALYSIS_LOWPASS_FILTER_3 -.02384946501938
#define CDF97_ANALYSIS_LOWPASS_FILTER_4  .037828455506995

#define CDF97_ANALYSIS_HIGHPASS_FILTER_0 -.788485616405660
#define CDF97_ANALYSIS_HIGHPASS_FILTER_1  .418092273222210
#define CDF97_ANALYSIS_HIGHPASS_FILTER_2  .040689417609558
#define CDF97_ANALYSIS_HIGHPASS_FILTER_3 -.064538882628938

#define CDF97_SYNTHESIS_LOWPASS_FILTER_0  .788485616405660
#define CDF97_SYNTHESIS_LOWPASS_FILTER_1  .377402855612650
#define CDF97_SYNTHESIS_LOWPASS_FILTER_2 -.040689417609558
#define CDF97_SYNTHESIS_LOWPASS_FILTER_3 -.02384946501938

#define CDF97_SYNTHESIS_HIGHPASS_FILTER_0 -.85269867900940
#define CDF97_SYNTHESIS_HIGHPASS_FILTER_1  .418092273222210
#define CDF97_SYNTHESIS_HIGHPASS_FILTER_2  .110624404418420
#define CDF97_SYNTHESIS_HIGHPASS_FILTER_3 -.064538882628938
#define CDF97_SYNTHESIS_HIGHPASS_FILTER_4 -.037828455506995

/*
  Simple implementation of a discrete wavelet transform using the CPU.
*/

unsigned countLeadingZeros(unsigned x);
unsigned ceilLog2(unsigned x);

// Returns the maximum number of steps a DWT can take for a given input length
// Is essentially ceil(log2(length))
int dwtMaximumSteps(int length);

bool is_padded_for_wavelet(int length, int steps);
bool is_padded_for_wavelet(scu_wavelet::int3 size, scu_wavelet::int3 steps);

// Transpose a square matrix.
void transpose_square(int size, float data[]);
void transpose_square(int size, double data[]);

/*
  Transpose an upper-left square of a square matrix.

  transpose_square_submatrix(4, 2, data):  
    a b . .       a c . .
    c d . .   ->  b d . .
    . . . .       . . . .
    . . . .       . . . .

    All "." cells would be unchanged.
*/
void transpose_square_submatrix(int total_size, int submatrix_size,
                                float data[]);

// print a matrix (for debugging purposes)
void print_matrix(int width, int height, float *data);

// Haar wavelet filter on one row of data, and the inverse.
// stepCount is the number of passes over the data. Values <= 0 will
// result in ceil(log2(data)) passes.
void haar(int length, float data[], bool inverse = false,
          int stepCount = -1);
void haar(int length, double data[], bool inverse = false,
          int stepCount = -1);

// Haar wavelet transform. on a 2-d square of data
// Returns the time the operation took in milliseconds.
float haar_2d(int size, float *data,
              bool inverse = false, int stepCount = -1,
              bool standardTranspose = false);

float haar_2d(int size, double *data,
              bool inverse = false, int stepCount = -1,
              bool standardTranspose = false);

// 3-d Haar
void haar_3d(CubeFloat *data, scu_wavelet::int3 stepCount,
             bool inverse = false, bool standardTranspose = false);

typedef enum {
  ZERO_FILL,  // fill pad elements with zero
  REFLECT,    // fill with reflection: abcde -> abcdedcb
  REPEAT      // fill with copies of last value: abcde->abcdeeee
} DWTPadding;


/*
  Given an input length, return that length rounded up to a length
  compatible with 'stepCount' steps of discrete wavelet transforms.
  If powerOfTwo is true, round up to a power of two. Otherwise,
  round up to a multiple of 2^stepCount. Return the rounded up length.
*/
int dwt_padded_length(int length, int stepCount, bool powerOfTwo);


/*
  Pad an array to the given length with the given padding method.

  The output array is returned. If output is NULL, a new array will be
  allocated. If inputLen==0, then the output array will be zero-filled.
*/
float *dwt_pad(int inputLen, float input[], 
	       int outputLen, float *output,
	       DWTPadding pad);
double *dwt_pad(int inputLen, double input[], 
		int outputLen, double *output,
		DWTPadding pad);

float *dwt_pad_2d(int rows, int cols, int rowPitch, float *input,
		  int outputRows, int outputCols, int outputPitch,
		  float *output, DWTPadding pad);
double *dwt_pad_2d(int rows, int cols, int rowPitch, double *input,
		   int outputRows, int outputCols, int outputPitch,
		   double *output, DWTPadding pad);

// 1-d CDF 9.7 wavelet transform
void cdf97(int length, float *data, int stepCount, float *tempGiven = NULL);
void cdf97_inverse(int length, float *data, int stepCount, float *tempGiven = NULL);

// 3-d CDF 9.7
void cdf97_3d(CubeFloat *data, scu_wavelet::int3 stepCount,
              bool inverse = false, bool standardTranspose = true,
              bool quiet = false);



/*
  Wrap an array such that if you reference values beyond the ends
  of the array, the results will be mirrored array values.

  For example, given an array with 7 elements: 0 1 2 3 4 5 6
  Request array[0..6] and you'll get the usual values array[0..6].
  array[-1] returns array[1], array[-2] return array[2], etc.
  array[7] returns array[5], array[8] return array[4], etc.
*/
class MirroredArray {
  int length;  // length of the actual data
  const float *array;

public:
  HD MirroredArray(int length_, const float *array_)
    : length(length_), array(array_) {}

  HD float operator[] (int offset) const {

    // negative offset: mirror to a positive
    if (offset < 0) offset = -offset;

    // past the end: fold it back, repeat if necessary
    // try using modulo, see if it speeds this up
    while (offset >= length) {
      offset = length*2 - offset - 2;

      if (offset < 0) offset = -offset;
    }

    return array[offset];
  }

  HD void setLength(int len) {length = len;}
};


// Like MirroredArray, but simpler. Ask for an invalid index and you get 0.
class ZeroExtendedArray {
  float *array;
  int length;  // length of the actual data

public:
  HD ZeroExtendedArray(float *array_, int length_)
    : array(array_), length(length_) {}

  HD float operator[] (int offset) const {

    if (offset < 0 || offset >= length) return 0;

    return array[offset];
  }

  HD void setLength(int len) {length = len;}
};


#endif // __DWT_CPU_H__
