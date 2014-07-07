all: WaveletSampleImage.class test_haar_cpu normalize convert haar

NVCC=nvcc -arch sm_20

WaveletSampleImage.class: WaveletSampleImage.java
	javac $<

IS_CYGWIN=

ifeq "$(shell uname | head -c 9)" "CYGWIN_NT"

IS_CYGWIN=YES
HAAR_OBJS=haar.obj dwt_cpu.obj dwt_gpu.obj data_io.obj transpose_gpu.obj nixtimer.obj
# NVCC_LIBS=-lws2_32
%.obj: %.cc
	$(NVCC) -c $<
%.obj: %.cu
	$(NVCC) -c $<
CLASSPATH_DIR="$(shell cygpath --windows `pwd`)"

else

HAAR_OBJS=haar.o dwt_cpu.o dwt_gpu.o data_io.o transpose_gpu.o nixtimer.o
LIBS=-lrt
%.o: %.cc
	$(NVCC) -c $<
%.o: %.cu
	$(NVCC) -c $<
CLASSPATH_DIR=$(CURDIR)

endif

haar: $(HAAR_OBJS)
	nvcc -arch sm_30 -g $^ -o $@

test_haar_cpu: test_haar_cpu.cc dwt_cpu.cc data_io.cc
	gcc -Wall -g $^ -o $@ -lstdc++ $(LIBS)

normalize: normalize.cc data_io.cc
	gcc -Wall -g $^ -o $@ -lstdc++ $(LIBS)

convert: Makefile
	@echo Write $@ wrapper for \"java WaveletSampleImage\"
	@echo '#!/bin/sh' > convert
	@echo java -cp \"$(CLASSPATH_DIR)\" WaveletSampleImage \"\$$@\" >> conve\
rt
	chmod 755 convert

send: .senddevel

.senddevel: *.cu *.cc *.h *.java Makefile
	scp $? devel:tmp/Wavelets
	touch .senddevel

clean:
	rm -f *.class *.obj *.o *.exp *.lib *.pdb haar test_haar_cpu *~ \
	  convert

