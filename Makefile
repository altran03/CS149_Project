CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
SRCDIR = src
HEADERDIR = $(SRCDIR)/headers
OBJDIR = obj
BINDIR = bin

# Source files
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/disk.c \
          $(SRCDIR)/utils.c \
          $(SRCDIR)/directory_operations.c \
          $(SRCDIR)/file_operations.c \
          $(SRCDIR)/permissions.c

# Object files
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = $(BINDIR)/fms

.PHONY: all clean directories

all: directories $(TARGET)

directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)
	@echo "Build complete: $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -I$(HEADERDIR) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Clean complete"

run: $(TARGET)
	./$(TARGET)

