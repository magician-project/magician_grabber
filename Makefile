CC = gcc
CFLAGS = -Wall `pkg-config --cflags aravis-0.10`
LDFLAGS = `pkg-config --libs aravis-0.10` -lpthread -lm
TARGET = magician_grabber
SRC = multimodalgrabber.c arduinoSensor.c atiForceSensor.c gigeCameraSensor.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
