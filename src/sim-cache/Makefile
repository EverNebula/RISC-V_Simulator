OBJECT = main.o machine.o riscsim.o memory.o cache.o config.o predictor.o utils.o
INCLUDE = ../../include
CPP_FLAGS = -O2

sim : $(OBJECT)
	g++ -o sim $(OBJECT) -lboost_program_options $(CPP_FLAGS)
main.o : main.cpp machine.hpp
	g++ -c main.cpp -I$(INCLUDE) $(CPP_FLAGS)
memory.o : memory.cpp memory.hpp machine.hpp storage.hpp
	g++ -c memory.cpp $(CPP_FLAGS)
cache.o : cache.cpp cache.hpp machine.hpp storage.hpp
	g++ -c cache.cpp $(CPP_FLAGS)
riscsim.o : riscsim.cpp riscsim.hpp machine.hpp
	g++ -c riscsim.cpp $(CPP_FLAGS)
machine.o : machine.cpp machine.hpp memory.hpp cache.hpp predictor.hpp riscsim.hpp
	g++ -c machine.cpp $(CPP_FLAGS)
config.o : config.cpp config.hpp machine.hpp
	g++ -c config.cpp $(CPP_FLAGS)
predictor.o : predictor.hpp
	g++ -c predictor.cpp $(CPP_FLAGS)
utils.o : utils.cpp utils.hpp
	g++ -c utils.cpp $(CPP_FLAGS)
clean :
	rm sim $(OBJECT)
