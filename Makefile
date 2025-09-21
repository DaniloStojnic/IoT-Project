# Compiler i flags
CXX = g++
CXXFLAGS = -I/usr/local/include -I/usr/local/include/paho-mqttpp3 -std=c++11 -pthread
LDFLAGS = -L/usr/local/lib -lpaho-mqttpp3 -lpaho-mqtt3as

# Izvori i binarke
BINARIES = senzor ventilator mikrokontroler sirena aplication

.PHONY: all clean run_all

all: $(BINARIES)

# Pravila za binarke
senzor: senzor.cpp 
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

ventilator: ventilator.cpp 
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

mikrokontroler: mikrokontroler.cpp 
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

sirena: sirena.cpp 
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

aplication: aplication.cpp
	$(CXX) $^ -o $@ $(CXXFLAGS) $(LDFLAGS)

# Pokretanje svih u odvojenim terminalima
run_all: all
	gnome-terminal -- bash -c "./senzor; exec bash"
	gnome-terminal -- bash -c "./ventilator; exec bash"
	gnome-terminal -- bash -c "./mikrokontroler; exec bash"
	gnome-terminal -- bash -c "./sirena; exec bash"
	gnome-terminal -- bash -c "./aplication; exec bash"

# Očisti sve binarke i objekt fajlove
clean:
	rm -f $(BINARIES) *.o
