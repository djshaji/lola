JACK_LIBS = `pkg-config -cflags -libs jack gtk+-3.0`

all: main repro_ratatouille

main: lola.cc main.cc
	@echo "Building main..."
	g++ -o main main.cc lola.cc $(JACK_LIBS) -g --std=c++20 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function

repro_ratatouille: ../tests/repro_ratatouille.cc lola.cc
	@echo "Building Ratatouille repro..."
	g++ -I. -o repro_ratatouille ../tests/repro_ratatouille.cc lola.cc $(JACK_LIBS) -g --std=c++20 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
