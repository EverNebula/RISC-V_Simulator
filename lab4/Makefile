OBJECT = main.o trans.o utils.o
INCLUDE = .
CPP_FLAGS = -O2 -march=native

trans : $(OBJECT)
	g++ -o trans $(OBJECT) -lboost_program_options $(CPP_FLAGS) -I$(INCLUDE)
main.o : main.cpp trans.hpp utils.hpp
	g++ -c main.cpp $(CPP_FLAGS) -I$(INCLUDE)
trans.o : trans.cpp trans.hpp
	g++ -c trans.cpp $(CPP_FLAGS) -I$(INCLUDE)
utils.o : utils.cpp utils.hpp
	g++ -c utils.cpp $(CPP_FLAGS) -I$(INCLUDE)
clean :
	rm trans $(OBJECT)
