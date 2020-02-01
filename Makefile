INCLUDE = ./include
LIBRARY = ./lib
GCC = riscv64-unknown-elf-gcc
C_FLAGS = -lecall -march=rv64i

cache :
	make -C src/sim-cache
	mv ./src/sim-cache/sim ./sim-cache

pipeline :
	make -C src/sim-pipeline
	mv ./src/sim-pipeline/sim .

single :
	make -C src/sim-single
	mv ./src/sim-single/sim .

cachetest :
	make -C src/cache-test
	mv ./src/cache-test/cachetest .

libecall :
	$(GCC) ./src/syscall.c -c -o ./src/syscall.o -I$(INCLUDE)
	ar -cr ./src/libecall.a ./src/syscall.o
	mv ./src/libecall.a $(LIBRARY)

testprog :
	$(GCC) ./src/test_prog/add.c -o ./test/add -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)
	$(GCC) ./src/test_prog/mul-div.c -o ./test/mul-div -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)
	$(GCC) ./src/test_prog/nl.c -o ./test/nl -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)
	$(GCC) ./src/test_prog/simple-fuction.c -o ./test/simple-fuction -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)
	$(GCC) ./src/test_prog/qsort.c -o ./test/qsort -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)
	$(GCC) ./src/test_prog/matmul.c -o ./test/matmul -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)
	$(GCC) ./src/test_prog/ackermann.c -o ./test/ackermann -I$(INCLUDE) -L$(LIBRARY) $(C_FLAGS)

all: pipeline libecall testprog

clean :
	find . -name "*.o"  | xargs rm -f
	rm sim
	rm sim-cache
