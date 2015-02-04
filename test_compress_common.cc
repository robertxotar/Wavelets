/**
   Test the full path of tools:
     Read data file
     Perform wavelet transform
     Apply cutoff threshold
     Quantize
     Run-length encode
     Save to file

  And the inverse.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "nixtimer.h"
#include "rle.h"
#include "param_string.h"
#include "test_compress_common.h"
#include "huffman.h"
#include "quant.h"
#include "cubelet_file.h"

using namespace std;
using namespace scu_wavelet;

// global variable that disables status output
bool QUIET = false;

// read&write just the data part of the file to&from f.intData
// using Huffman encoding
static void initHuffman(Huffman &huff, const CubeInt *cube, bool quiet);

static bool writeQuantDataHuffman(Huffman &huff, vector<uint32_t> *outData,
                                  const CubeInt *data, bool quiet);


void printHelp() {
  printf("\n"
         "  test_compress [opts] <input file> <output file>\n"
         "  Options:\n"
	 "    -d : decompress (default is to compress)\n"
         "    -steps <n> : # of wavelet transform steps (default = %d)\n"
         "    -thresh <n> : proportion of data to be eliminated in threshold cutoff\n"
         "                  Must be between [0..1]. (default = %.3f)\n"
         "    -qcount <n> : # of bins into which data is quantized.\n"
         "                 Must be >= 1 (default=%d)\n"
         "    -wave <wavelet> : Wavelet to use: haar or cdf97. (default = %s)\n"
         "    -qalg <alorithm> : quantization algorithm: uniform, log, count, or lloyd\n"
         "                       (default = %s)\n"
         "    -opt : enable parameter optimization\n"
         "           this will automatically find the threshold and bin count\n"
         "    -bq <filename> : before quantizing, save a copy of the data this file\n"
         "    -enc : print the bit encoding of each value\n"
         "    -nonstd : use nonstandard wavelet transpose order\n"
         "    -err : compute error metrics (slow, disabled by default)\n"
         "    -q : be quiet; suppess all output\n"
         "    -v : be verbose; print the data after each step\n"
         "\n",
         DEFAULT_WAVELET_STEPS,
         DEFAULT_THRESHOLD_FRACTION,
         DEFAULT_QUANTIZE_BINS,
         waveletAlgToName(DEFAULT_WAVELET),
         quantAlgId2Name(DEFAULT_QUANTIZE_ALGORITHM)
         );

  // defaults are defined in wavelet.h, just after enum declarations

  exit(1);
}


bool parseOptions(int argc, char **argv, Options &opt, int &nextArg) {
  opt.init();

  for (nextArg = 1; nextArg < argc; nextArg++) {
    const char *arg = argv[nextArg];
    if (arg[0] != '-') break;

    if (!strcmp(arg, "-d")) {
      opt.doCompress = false;
    }

    else if (!strcmp(arg, "-steps")) {
      if (++nextArg >= argc) printHelp();
      arg = argv[nextArg];
      int steps;
      if (1 != sscanf(arg, "%d", &steps) || steps < 0) {
        fprintf(stderr, "Invalid # of wavelet transform steps: \"%s\"\n", arg);
        return false;
      } else {
        opt.param.transformSteps = int3(steps, steps, steps);
      }
    }

    else if (!strcmp(arg, "-thresh")) {
      if (++nextArg >= argc) printHelp();
      arg = argv[nextArg];
      if (1 != sscanf(arg, "%f", &opt.param.thresholdFraction) ||
          opt.param.thresholdFraction < 0 ||
          opt.param.thresholdFraction > 1) {
        fprintf(stderr, "Invalid threshold proportion: \"%s\"\n", arg);
        return false;
      }
    }

    else if (!strcmp(arg, "-qcount")) {
      if (++nextArg >= argc) printHelp();
      arg = argv[nextArg];
      if (1 != sscanf(arg, "%d", &opt.param.binCount) ||
          opt.param.binCount < 1) {
        fprintf(stderr, "Invalid # quantize bins: \"%s\"\n", arg);
        return false;
      }
    }

    else if (!strcmp(arg, "-bq")) {
      if (++nextArg >= argc) printHelp();
      opt.saveBeforeQuantizingFilename = argv[nextArg];
    }

    else if (!strcmp(arg, "-qalg")) {
      if (++nextArg >= argc) printHelp();
      opt.param.quantAlg = quantAlgName2Id(argv[nextArg]);
      if (opt.param.quantAlg == QUANT_ALG_UNKNOWN) {
        fprintf(stderr, "Invalid quantize algorithm: \"%s\"\n", argv[nextArg]);
        return false;
      }
    }

    else if (!strcmp(arg, "-wave")) {
      if (++nextArg >= argc) printHelp();
      opt.param.waveletAlg = waveletAlgNameToId(argv[nextArg]);
      if (opt.param.waveletAlg == WAVELET_UNKNOWN) {
        fprintf(stderr, "Invalid wavelet: \"%s\"\n", argv[nextArg]);
        return false;
      }
    }

    else if (!strcmp(arg, "-opt")) {
      opt.doOptimize = true;
    }

    else if (!strcmp(arg, "-enc")) {
      opt.printHuffmanEncoding = true;
    }

    else if (!strcmp(arg, "-nonstd")) {
      opt.param.isWaveletTransposeStandard = false;
    }

    else if (!strcmp(arg, "-q")) {
      opt.quiet = true;
    }

    else if (!strcmp(arg, "-v")) {
      opt.verbose = true;
    }

    else if (!strcmp(arg, "-err")) {
      opt.doComputeError = true;
    }

    else if (!strcmp(arg, "-experiment")) {
      opt.runQuantizationExperiments = true;
      opt.quiet = true;
    }

    else {
      fprintf(stderr, "Unrecognized option: \"%s\"\n", arg);
      return false;
    }
  }

  return true;
}


/**
   Write the data as one cubelet to the cubelet stream.
*/
bool writeQuantData(CubeletStreamWriter &cubeletStream,
                    CubeInt *cube, Options &opt,
                    int *sizeBytes) {
  
  if (sizeBytes) *sizeBytes = 0;

  // initialize the huffman encoding
  Huffman huff;
  initHuffman(huff, cube, opt.quiet);
  if (opt.printHuffmanEncoding) huff.printEncoding();

  vector<uint32_t> encodedData;

  if (!writeQuantDataHuffman(huff, &encodedData, cube, opt.quiet))
    return false;

  huff.getDecoderTable(cube->param.huffDecode);
  cube->param.compressedSize = (int)(encodedData.size() * sizeof(uint32_t));
  cube->isWaveletCompressed = true;

  // temporarily swap in the compressed data
  void *saveData = cube->data_;
  cube->data_ = encodedData.data();
  
  cubeletStream.addCubelet(cube);

  cube->data_ = saveData;

  // return success;
  return true;
}


// returns true iff 'suffix' is a suffix of 's'
bool endsWith(const char *s, const char *suffix) {
  int len = (int) strlen(s), suffixLen = (int) strlen(suffix);
  if (suffixLen > len) return false;
  return 0 == strcmp(s + len - suffixLen, suffix);
}


/**
   Read a cubelet from the input file.
     inputData will contain the original data
     data will contain a copy of the data, padded appropriately for the
       the number of transform steps in param->transformSteps
       (for n steps, pad up to a multiple of 2^n)
     How param is used:
       transformSteps is read to do the padding
       originalSize is set
       originalDatatype is set

   If the filename ends with ".cube":
     It is assumed to be a cubelet file, and is read with
     cubeletStream. cubeletStream will be opened if it isn't
     already. It will not be closed, so that more cubelets can be read.

   If the filename doesn't end with ".cube":
     It will be read with "readDataFile" from data_io.h.

   On error, output an error message and return false
*/
bool readData(const char *filename,
              Cube *inputData,  // the original data, may not be floats
              CubeFloat *data,  // input data transformed into floats
              CubeletStreamReader *cubeletStream,
              WaveletCompressionParam *param) {

  double startTime = NixTimer::time();

  bool isCubeletFile = endsWith(filename, ".cube");

  // get the size of the data without reading the data
  if (!isCubeletFile) {

    if (!readDataFile(filename, (float**)NULL,
                      &inputData->size.x, &inputData->size.y))
      return false;
    inputData->size.z = 1;
    inputData->datatype = WAVELET_DATA_FLOAT32;

  } else {

    // open the stream if it isn't already
    if (!cubeletStream->isOpen()) {
      if (!cubeletStream->open(filename)) return false;
    }

    // find the next uncompressed cubelet
    do {
      if (!cubeletStream->next(inputData)) {
        fprintf(stderr, "No uncompressed cubelets in the input data.\n");
        return false;
      }

    } while (inputData->isWaveletCompressed);

    // note: inputData.datatype might not be FLOAT32
  }

  inputData->allocate();
  
  // save the original size of the input data
  param->originalSize = inputData->size;
  param->originalDatatype = inputData->datatype;

  // adjust the number of wavelet steps in case the user requested too many
  setWaveletSteps(param->transformSteps, inputData->size);

  // Pad the size of the data (without touching the data itself, because
  // it hasn't been loaded yet) to align with the number of transformation
  // steps we'll be doing.
  int3 paddedSize = inputData->size;
  padDataSize(paddedSize, param->transformSteps);
  data->size = paddedSize;
  data->allocate();

  // read the actual data
  if (!isCubeletFile) {
    assert(inputData->datatype == WAVELET_DATA_FLOAT32);

    float *array;
    int width, height;
    if (!readDataFile(filename, &array, &width, &height)) return false;

    assert(inputData->size == int3(width, height, 1));
    memcpy(inputData->data_, array, sizeof(float)*width*height);
    data->copyFrom(array, inputData->size);
    delete[] array;

  } else { // read cubelet entry

    if (!cubeletStream->getCubeData(inputData)) return false;

    // translate the cube data if necessary
    translateCubeDataToFloat(inputData, data);

  }

  double elapsed = NixTimer::time() - startTime;
  if (!QUIET) {
    printf("Read %dx%dx%d data file: %.2f ms\n",
           inputData->size.x, inputData->size.y, inputData->size.z,
           elapsed * 1000);
    fflush(stdout);
  }

  padData(*data, inputData->size);

  return true;
}


// If the user specified too many steps or a negative number of steps,
// set them to the maximum number given the data size.
void setWaveletSteps(int3 &steps, const int3 &size) {
  int maxSteps = dwtMaximumSteps(size.x);
  if (steps.x < 0 || steps.x > maxSteps)
    steps.x = maxSteps;

  maxSteps = dwtMaximumSteps(size.y);
  if (steps.y < 0 || steps.y > maxSteps)
    steps.y = maxSteps;

  maxSteps = dwtMaximumSteps(size.z);
  if (steps.z < 0 || steps.z > maxSteps)
    steps.z = maxSteps;
}

// Find the size to which the data needs to be padded so each transform
// step has an even number of elements. Change the given size in-place.
void padDataSize(int3 &size, int3 steps) {
  size.x = dwt_padded_length(size.x, steps.x, false);
  size.y = dwt_padded_length(size.y, steps.y, false);
  size.z = dwt_padded_length(size.z, steps.z, false);
}


// REPEAT-pad data
void padData(CubeFloat &data, int3 originalSize) {
  
  if (data.size == originalSize) return;
  assert(data.size >= originalSize);

  double startTime = NixTimer::time();

  // for each existing layer of data, pad the end of each row and column
  for (int z = 0; z < originalSize.z; z++) {

    // pad each row by replicating the last value in each row
    for (int y = 0; y < originalSize.y; y++) {
      float value = data.get(originalSize.x-1, y, z);
      float *p = data.pointer(originalSize.x, y, z);
      float *e = p + data.width() - originalSize.x;
      while (p < e) *p++ = value;
    }

    // pad each column by replicating the last row
    float *rowp = data.pointer(0, originalSize.y-1, z);
    for (int y = originalSize.y; y < data.height(); y++)
      memcpy(data.pointer(0, y, z), rowp, data.width() * sizeof(float));
  }

  // for the new layers of data, copy the last layer of data
  float *layerp = data.pointer(0, 0, originalSize.z-1);
  int layerSize = data.width() * data.height() * sizeof(float);
  for (int z=originalSize.z; z < data.depth(); z++)
    memcpy(data.pointer(0, 0, z), layerp, layerSize);

  double elapsed = NixTimer::time() - startTime;
  if (!QUIET)
    printf("Pad data from %dx%dx%d to %dx%dx%d: %.2f ms\n",
           originalSize.x, originalSize.y, originalSize.z, 
           data.width(), data.height(), data.depth(),
           elapsed*1000);

}


class RowByteToFloat {
  CubeFloat *dest;
public:
  RowByteToFloat(CubeFloat *d) : dest(d) {}

  void visitRow(const unsigned char *readp, int length, int y, int z) {
    const unsigned char *end = readp + length;
    float *writep = dest->pointer(0, y, z);
    while (readp < end) {
      *writep++ = *readp++ * (1.0f / 255) - 0.5f;
    }
  }
};

class RowIntToFloat {
  CubeFloat *dest;
public:
  RowIntToFloat(CubeFloat *d) : dest(d) {}

  void visitRow(const int *readp, int length, int y, int z) {
    const int *end = readp + length;
    float *writep = dest->pointer(0, y, z);
    while (readp < end) {
      *writep++ = (float) *readp++;
    }
  }
};


// make the cubelet data into floats, if it isn't already
void translateCubeDataToFloat(Cube *src, CubeFloat *dest) {

  // check that there are no insets
  assert(src->size == src->totalSize);

  // copy source cubelet id
  dest->parentOffset = src->parentOffset;

  if (src->datatype == WAVELET_DATA_FLOAT32) {
    dest->copyFrom(*(const CubeFloat*)src);
    return;
  }

  // check that dest's memory is already allocated
  assert(dest->data_);

  // int count = src->count();
  // float *writep = dest->pointer(0,0,0);
  
  // map 0..255 to -.5 .. .5
  if (src->datatype == WAVELET_DATA_UINT8) {
    CubeByte *s = (CubeByte*) src;
    RowByteToFloat rowVisitor(dest);
    s->visitRows(rowVisitor);
  }

  // without any other range information, just translate directly to floats
  else if (src->datatype == WAVELET_DATA_INT32) {
    CubeInt *s = (CubeInt*) src;
    RowIntToFloat rowVisitor(dest);
    s->visitRows(rowVisitor);
  }
}


// Peform the wavelet transform
bool waveletTransform(CubeFloat &data, const WaveletCompressionParam &param,
                      bool isInverse, bool verbose) {

  // do nothing if 0 steps in each direction
  if (param.transformSteps == int3(0,0,0)) return true;
  
  double startTime = NixTimer::time();

  if (verbose) data.print("Before wavelet transform");

  if (param.waveletAlg == WAVELET_CDF97) {
    cdf97_3d(&data, param.transformSteps, isInverse, 
             param.isWaveletTransposeStandard);
  }

  else if (param.waveletAlg == WAVELET_HAAR) {
    haar_3d(&data, param.transformSteps, isInverse, 
            param.isWaveletTransposeStandard);
  }

  else {
    fprintf(stderr, "Unknown wavelet: %s\n",
            waveletAlgToName(param.waveletAlg));
    return false;
  }

  if (verbose) data.print("After wavelet transform");

  double elapsedMs = (NixTimer::time() - startTime) * 1000;

  if (!QUIET)
    printf("%s%s wavelet transform (%d,%d,%d steps): %.2f ms\n", 
           waveletAlgToName(param.waveletAlg),
           isInverse ? " inverse" : "",
           param.transformSteps.x, param.transformSteps.y,
           param.transformSteps.z, elapsedMs);

  return true;
}


bool dequantize(const CubeInt &quantizedData, CubeFloat &data,
                const WaveletCompressionParam &param) {

  double startTime = NixTimer::time();
  Quantizer *quantizer = createQuantizer(param);
  if (!quantizer) return false;

  quantizer->dequantizeRow(quantizedData.pointer(0,0,0), data.pointer(0,0,0),
                           quantizedData.count());

  if (!QUIET)
    printf("Dequantize %s: %.2f ms\n", quantAlgId2Name(param.quantAlg),
           (NixTimer::time() - startTime) * 1000);

  delete quantizer;
  
  return true;
}


void computeErrorRates(const CubeInt *quantizedData,
                       const WaveletCompressionParam &param,
                       const Cube *inputData,
                       float *meanSquaredError,
                       float *peakSNR) {

  // dequantize quantizedData into restoredData
  CubeFloat restoredData;
  restoredData.size = quantizedData->size;
  restoredData.allocate();
  if (!dequantize(*quantizedData, restoredData, param)) return;

  ErrorAccumulator errAccum;

  computeErrorRatesAfterDequant(&restoredData, param, inputData, &errAccum);

  *meanSquaredError = errAccum.getMeanSquaredError();
  *peakSNR = errAccum.getPeakSignalToNoiseRatio();
}


/**
   Compare the compressed data with the original data.
   Note: restoredData will be modified in-place.
*/
void computeErrorRatesAfterDequant
(CubeFloat *restoredData,
 const WaveletCompressionParam &param,
 const Cube *inputData,
 ErrorAccumulator *errAccum) {

  if (inputData->datatype != WAVELET_DATA_UINT8) {
    fprintf(stderr, "Current implementation can only compute error rates if input data is unsigned bytes (0..255)\n");
    return;
  }

  // perform the inverse wavelet transform on restoredData
  // restoredData->print("Before transform");
  if (!waveletTransform(*restoredData, param, true, false)) return;
  // restoredData->print("After transform");

  const int width = inputData->width();
  const CubeByte *original = (const CubeByte *) inputData;

  errAccum->setMaxPossible(255);  // largest unsigned char value

  // for each row of data in the original data, compare pixels
  for (int z=0; z < original->depth(); z++) {
    for (int y=0; y < original->height(); y++) {
      const unsigned char *originalRow = original->pointer(0, y, z);
      const float *restoredRow = restoredData->pointer(0, y, z);
      
      for (int x=0; x < width; x++) {
        float f = (restoredRow[x] + 0.5f) * 255;
        if (f < 0) {
          f = 0;
        } else if (f > 255) {
          f = 255;
        }
        // add half to round rather than truncate
        unsigned char pixelValue = (unsigned char) (f + .5f);

        errAccum->add(originalRow[x], pixelValue);
      }
    }
  }

}


// turn the data back into the original datatype, if it wasn't floats
// dest->size has been set, and it may be smaller than src
void translateCubeDataToOriginal(CubeFloat *src, Cube *dest, bool verbose) {

  // check that there are no insets
  assert(src->size == src->totalSize);

  // copy source size
  dest->totalSize = src->totalSize;
  dest->parentOffset = src->parentOffset;

  if (dest->datatype == src->datatype) {
    dest->data_ = src->data_;
    src->data_ = NULL;
    dest->ownsData = src->ownsData;
    return;
  }

  dest->allocate();

  int count = src->count();
  float *readp = src->pointer(0,0,0), *endp = readp + count;
  
  // map -.5 .. .5 to 0..255
  if (dest->datatype == WAVELET_DATA_UINT8) {
    CubeByte *d = (CubeByte*) dest;
    unsigned char *writep = d->pointer(0,0,0);

    while (readp < endp) {
      // add half to round rather than truncate
      int x = (int) ((*readp++ + 0.5f) * 255 + 0.5f);
      if (x < 0) {
        x = 0;
      } else if (x > 255) {
        x = 255;
      }
      *writep++ = x;
    }
    if (verbose) d->print("After restoring data type");
  }

  // without any other range information, just translate directly to ints
  else if (dest->datatype == WAVELET_DATA_INT32) {
    CubeInt *d = (CubeInt*) dest;
    int *writep = d->pointer(0,0,0);

    // map 0..255 to -.5 .. .5
    while (readp < endp) {
      *writep++ = (int)*readp++;
    }

    if (verbose) d->print("After restoring data type");
  }

  src->deallocate();

}


// Read compressed, quantized data from a cublet stream.
bool readQuantData(CubeletStreamReader &cubeletStream, CubeInt *cube) {

  while (true) {
    if (!cubeletStream.next(cube)) {
      fprintf(stderr, "No compressed cubelet found\n");
      return false;
    }

    // look for a compressed cubelet in the right format
    if (cube->datatype == WAVELET_DATA_INT32 &&
        cube->isWaveletCompressed)
      break;
  }

  int wordCount = cube->param.compressedSize / sizeof(uint32_t);
  vector<uint32_t> bitData(wordCount, 0);

  // read the compressed data into an array of uint32_t's
  if (!cubeletStream.getRawData(bitData.data()))
    return false;

  // allocate storage for the decoded integer values
  cube->allocate();

  HuffmanDecoder huffDecoder;
  huffDecoder.init(cube->param.huffDecode);

  BitStreamMemorySource mem(&bitData);
  BitStreamReader<BitStreamMemorySource> bitReader(&mem);

  assert(cube->count());
  assert(cube->data());

  int readCount = huffDecoder.decodeFromStream
    (cube->data(), cube->count(), &bitReader);

  if (cube->count() != readCount) {
    printf("ERROR: read only %d of %d values\n", readCount, cube->count());
    return false;
  }

  return true;
}


class TraverseForHuffman {
public:
  Huffman &huff;
  TraverseForHuffman(Huffman &h) : huff(h) {}
  
  void visit(int value) {
    huff.increment(value);
  }
};

static void initHuffman(Huffman &huff, const CubeInt *cube, bool quiet) {

  double startTime = NixTimer::time();

  // number of possible values
  huff.init(cube->param.binCount);

  // train the huffman encoder
  TraverseForHuffman init(huff);
  cube->visit<TraverseForHuffman>(init);

  huff.computeHuffmanCoding();
  double elapsed = NixTimer::time() - startTime;
  if (!quiet)
    printf("Huffman build table %.3f ms\n", elapsed*1000);
}


static bool writeQuantDataHuffman(Huffman &huff, vector<uint32_t> *outData,
                                  const CubeInt *data, bool quiet) {

  BitStreamMemorySink memorySink(outData);
  BitStreamWriter<BitStreamMemorySink> bitWriter(&memorySink);

  // this routine does not accept padded data
  assert(data->inset == int3(0,0,0));
  huff.encodeToStream(&bitWriter, data->pointer(0,0,0), data->count());

  // printf("%llu bits written\n", (long long unsigned) bitWriter.size());
  size_t bitsWritten = bitWriter.size();
  int bytesWritten = (int)((bitsWritten + 31) / 32 * 4);

  long long unsigned totalBits = 0;
  for (int i=0; i < data->param.binCount; i++)
    totalBits += huff.encodedLength(i) * huff.getCount(i);

  if (!quiet)
    printf("Huffman encoding: %d bytes, %.2f bits/pixel, "
           "longest encoding = %d bits\n",
           bytesWritten, (double)totalBits / data->count(),
           huff.getLongestEncodingLength());

  return true;
}


Quantizer *createQuantizer(const WaveletCompressionParam &param) {

  switch (param.quantAlg) {
  case QUANT_ALG_LOG:
    return new QuantizerLog(param.binCount, param.thresholdValue, param.maxValue);
  case QUANT_ALG_UNIFORM:
    return new QuantizerUniform(param.binCount, param.thresholdValue, param.maxValue);
  case QUANT_ALG_LLOYD:
    return new QuantizerCodebook(param.binValues);
  default:
    fprintf(stderr, "Unknown quantization algorithm id: %d\n",
            (int)param.quantAlg);
    return NULL;
  }
}
