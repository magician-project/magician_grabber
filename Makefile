CC = gcc
SRC_DIR = src
CFLAGS = -Wl,--copy-dt-needed-entries -O2 -fPIC -fPIE -Wall -Wno-unused-function `pkg-config --cflags aravis-0.10` -lrt
LDFLAGS = `pkg-config --libs aravis-0.10` -lpthread -lm -lpng

CFLAGS_SHARED  = -Wall -pthread -lrt -lm -g
LDFLAGS_SHARED = -shared -fPIC -g
TARGET = magician_grabber

CPP = g++
CPPFLAGS = $(CFLAGS)
TARGET_TACTILE = magician_grabber_tactile
# -pg
TACTILE_DEBUG   = -pg -Wstrict-overflow -fsanitize=address -fPIE -fPIC -DTACTILE -DTACTILE_LIBRARY $(SRC_DIR)/tactile_processor/TactileProcessor.cpp -lfftw3  -L: $(SRC_DIR)/tactile_processor/iir1/build/libiir_static.a  
TACTILE_RELEASE = -DTACTILE -DTACTILE_LIBRARY $(SRC_DIR)/tactile_processor/TactileProcessor.cpp -lfftw3  -L: $(SRC_DIR)/tactile_processor/iir1/build/libiir_static.a  
SRC = $(addprefix $(SRC_DIR)/, multiModalGrabber.c arduinoSensor.c atiForceSensor.c gigeCameraSensor.c sharedMemoryVideoBuffers.c imageStreamer.c tactileFeatures.c tactileStreamer.c polarizationLights.c codecs/pngInput.c)

LIBRARY_SRC   = $(SRC_DIR)/sharedMemoryVideoBuffers.c
LIBRARY_OBJ   = $(addprefix $(OBJ_DIR)/, $(notdir $(LIBRARY_SRC:.c=.o)))
LIBRARY_NAME  = libSharedMemoryVideoBuffers.so

all: $(TARGET) $(TARGET_TACTILE) $(LIBRARY_NAME)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)

$(TARGET_TACTILE): $(SRC)
	$(CPP) -o $(TARGET_TACTILE) $(SRC) $(CPPFLAGS) $(LDFLAGS) $(TACTILE_RELEASE)

$(LIBRARY_NAME): $(LIBRARY_SRC)
	$(CC) $(LDFLAGS_SHARED) -o $(LIBRARY_NAME) $(LIBRARY_SRC) $(CFLAGS_SHARED) 

clean:
	rm -f $(TARGET) $(TARGET_TACTILE)
