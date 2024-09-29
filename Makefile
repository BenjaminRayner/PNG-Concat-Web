
# Makefile, ECE252  
# Yiqing Huang 

CC = gcc
CFLAGS = -Wall -g -std=gnu99
LD = gcc
LDFLAGS = -g 
LDLIBS = -lcurl -lz -pthread

LIB_UTIL = zutil.o crc.o
SRCS   = main.c main_write_header_cb.c catpng.c crc.c zutil.c
OBJS   = main.o $(LIB_UTIL)
TARGETS= paster

all: ${TARGETS}

paster: $(OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) 
