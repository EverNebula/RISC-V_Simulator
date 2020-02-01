OBJECT = main.o memory.o utils.o
INCLUDE = ../../include
CPP_FLAGS = -O2

cachetest : $(OBJECT)
	g++ -o cachetest $(OBJECT) -lboost_program_options $(CPP_FLAGS)
main.o : main.cpp memory.hpp
	g++ -c main.cpp -I$(INCLUDE) $(CPP_FLAGS)
memory.o : memory.cpp memory.hpp
	g++ -c memory.cpp $(CPP_FLAGS)
utils.o : utils.cpp utils.hpp
	g++ -c utils.cpp $(CPP_FLAGS)
clean :
	rm cachetest $(OBJECT)
