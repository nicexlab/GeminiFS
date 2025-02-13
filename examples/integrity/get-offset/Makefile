# Makefile  
CC=gcc  
CFLAGS=-Wall -Wextra  
LDFLAGS= -lpthread  
LDLIBS=  
  


# Target executable name  
TARGET = get-offset
# Object files  
OBJS = get-offset.o 
  

  
all: $(TARGET)  
  
$(TARGET): $(OBJS)  
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(LIBURING_PATH)  
  
%.o: %.c  
	$(CC) $(CFLAGS) -c $< -o $@  
  
clean:  
	rm -f *.o $(TARGET)  
  
.PHONY: clean