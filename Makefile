CC = gcc
CFLAGS = -Wall -g
LIBS = -lavformat -lavcodec -lavutil -lswscale
TARGET = frame_extractor
SRC = frame_extractor.c

# Build the executable
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

# Clean up generated files
clean:
	rm -f $(TARGET) *.jpg

