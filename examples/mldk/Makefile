NVCCFLAGS=-arch=sm_80 -I../../lib -I../src/include -I../../src/ -I../../src/linux -I../../src/include -I../../src/freestanding/include -O3 $(CXXFLAGS)
LDFLAGS=-L../../build/lib -Xlinker -rpath=$(shell pwd)/../../build/lib -lgeminiFs -lnvm -arch=sm_80

all:TestForNvmeBacking.exe

clean:
	rm -f *.exe

TestForNvmeBacking.exe: TestForNvmeBacking.cu
	nvcc $(LDFLAGS) $(NVCCFLAGS) TestForNvmeBacking.cu -o TestForNvmeBacking.exe