# Executable output name
TARGET = network

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall `pkg-config --cflags gtk+-3.0` -rdynamic

# Linker flags
LDFLAGS = `pkg-config --libs gtk+-3.0`

# Source files
SRCS = main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: clean $(TARGET)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files into the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Clean up object files and executable
clean:
	rm -f $(OBJS) $(TARGET)