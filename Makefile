CC = gcc
CFLAGS = -Wl,--copy-dt-needed-entries -O2 -fPIC -fPIE -Wall -Wno-unused-function `pkg-config --cflags aravis-0.10`
LDFLAGS = `pkg-config --libs aravis-0.10` -lpthread -lm
TARGET = magician_grabber

CPP = g++
TARGET_TACTILE = magician_grabber_tactile
TACTILE = -pg -Wstrict-overflow -fsanitize=address -fPIE -fPIC -DTACTILE -DTACTILE_LIBRARY tactile_processor/TactileProcessor.cpp -lfftw3  -L: tactile_processor/iir1/build/libiir_static.a  
TACTILEV = -g -fPIE -fPIC -DTACTILE -DTACTILE_LIBRARY tactile_processor/TactileProcessor.cpp -lfftw3  -L: tactile_processor/iir1/build/libiir_static.a  
SRC = multiModalGrabber.c arduinoSensor.c atiForceSensor.c gigeCameraSensor.c sharedMemoryVideoBuffers.c imageStreamer.c tactileFeatures.c

all: $(TARGET) $(TARGET_TACTILE)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

$(TARGET_TACTILE): $(SRC)
	$(CPP) $(CFLAGS) -o $(TARGET_TACTILE) $(SRC) $(LDFLAGS) $(TACTILE)

clean:
	rm -f $(TARGET) $(TARGET_TACTILE)
