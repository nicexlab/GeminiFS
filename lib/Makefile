CCFLAGS= -O3 -I/usr/local/cuda/include
NVCCFLAGS=-arch=sm_80 -Icccl/thrust -Icccl/libcudacxx/include -Icccl/cub  -I../src/include  -I../src/freestanding/include --expt-relaxed-constexpr --expt-extended-lambda -rdc=true -O3
LDFLAGS=-L. -L../build/lib -Xlinker -rpath=../build/lib -lgeminiFs -lnvm

all: libgeminiFs.a

clean:
	rm -f *.o  *.a *.exe

libgeminiFs.a: geminifs_nvme.cuda.o geminifs.o
	ar rcs libgeminiFs.a geminifs_nvme.cuda.o geminifs.o

geminifs_nvme.cuda.o: geminifs_api.h geminifs_nvme.cu
	nvcc $(NVCCFLAGS) geminifs_nvme.cu -c -o geminifs_nvme.cuda.o

geminifs.o: geminifs_api.h geminifs.c
	gcc $(CCFLAGS) geminifs.c -c -o geminifs.o
