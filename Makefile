all: xmlparser

action_list: action_list.hpp in_order_executor.hpp global_states.hpp exceptions.hpp parameters.hpp action_list.cpp macros.hpp extractor.hpp
	g++ -std=c++14 -O2 -c -o obj/action_list.o action_list.cpp

extractor: extractor.hpp action_list.hpp global_states.hpp exceptions.hpp parameters.hpp extractor.cpp macros.hpp
	g++ -std=c++14 -O2 -c -o obj/extractor.o extractor.cpp

global_states: global_states.hpp global_states.cpp
	g++ -std=c++14 -O2 -c -o obj/global_states.o global_states.cpp

in_order_executor: in_order_executor.hpp global_states.hpp exceptions.hpp in_order_executor.cpp
	g++ -std=c++14 -O2 -c -o obj/in_order_executor.o in_order_executor.cpp

main: action_list.hpp extractor.hpp splitter.hpp in_order_executor.hpp global_states.hpp exceptions.hpp parameters.hpp main.cpp
	g++ -std=c++14 -O2 -c -o obj/main.o main.cpp

splitter: splitter.hpp extractor.hpp global_states.hpp exceptions.hpp parameters.hpp splitter.cpp macros.hpp
	g++ -std=c++14 -O2 -c -o obj/splitter.o splitter.cpp

xmlparser: action_list extractor global_states in_order_executor splitter main
	g++ -std=c++14 -O2 -o xmlparser obj/action_list.o obj/extractor.o obj/global_states.o obj/in_order_executor.o obj/splitter.o obj/main.o -lboost_program_options -lpthread

clean:
	rm xmlparser
	rm obj/*.o
