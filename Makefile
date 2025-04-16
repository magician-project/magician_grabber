CC = gcc
CFLAGS = -Wl,--copy-dt-needed-entries -O2 -fPIC -fPIE -Wall -Wno-unused-function `pkg-config --cflags aravis-0.10` -lrt
LDFLAGS = `pkg-config --libs aravis-0.10` -lpthread -lm
TARGET = magician_grabber

CPP = g++
CPPFLAGS = $(CFLAGS)
TARGET_TACTILE = magician_grabber_tactile
# -pg
TACTILE_DEBUG   = -pg -Wstrict-overflow -fsanitize=address -fPIE -fPIC -DTACTILE -DTACTILE_LIBRARY tactile_processor/TactileProcessor.cpp -lfftw3  -L: tactile_processor/iir1/build/libiir_static.a  
TACTILE_RELEASE = -DTACTILE -DTACTILE_LIBRARY tactile_processor/TactileProcessor.cpp -lfftw3  -L: tactile_processor/iir1/build/libiir_static.a  
SRC = multiModalGrabber.c arduinoSensor.c atiForceSensor.c gigeCameraSensor.c sharedMemoryVideoBuffers.c imageStreamer.c tactileFeatures.c

all: $(TARGET) $(TARGET_TACTILE)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)

$(TARGET_TACTILE): $(SRC)
	$(CPP) -o $(TARGET_TACTILE) $(SRC) $(CPPFLAGS) $(LDFLAGS) $(TACTILE_RELEASE)

clean:
	rm -f $(TARGET) $(TARGET_TACTILE)
