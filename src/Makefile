JACK_LIBS = `pkg-config -cflags -libs jack gtk+-3.0`

all: main

main: lola.cc main.cc
	@echo "Building main..."
	g++ -o main main.cc lola.cc $(JACK_LIBS) -g --std=c++20 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
