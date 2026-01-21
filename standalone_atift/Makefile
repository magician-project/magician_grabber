CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS :=

TARGET  := atiToCSV
SRC     := atiToCSV.c
OBJ     := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJ)

