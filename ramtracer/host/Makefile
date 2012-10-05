UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
        # Default install location for libusb-1.0 on Mac OS
	CFLAGS += -I/opt/local/include/libusb-1.0 -I/opt/local/include
	LDFLAGS += -L/opt/local/lib -lusb-1.0 -lz -lreadline
	FLEX = /opt/local/bin/flex
	BISON = /opt/local/bin/bison
	
	# Large file support
	CFLAGS += -DFILE_OFFSET_BITS=64
else
        # Load libusb via pkg-config
	PACKAGES := libusb-1.0
	CFLAGS += $(shell pkg-config --cflags $(PACKAGES))
	LDFLAGS += $(shell pkg-config --libs $(PACKAGES))

	# Large file support
	CFLAGS += $(shell getconf LFS_CFLAGS)
endif

# Local headers
CFLAGS += -I../include

BIN := memhost
OBJS := main.o fastftdi.o fpgaconfig.o bit_file.o \
        hw_main.o hw_buffer.o hw_capture.o utils.o \
        hw_patch.o hw_config.o hw_process.o hw_command.o \
		hw_commandbuffer.o commandparser.o commandlex.o \
		hw_mempool.o hw_commandtype.o server.o debugger.o

CFLAGS += -O3 -g

all: $(BIN)

$(BIN): $(OBJS)
	cc -o $(BIN) $(OBJS) $(LDFLAGS)
	
commandlex.c : command.l
	$(FLEX) -o $@ $<

commandparser.c : command.y
	$(BISON) -d $< -o $@

*.o: *.h Makefile

clean:
	rm -f $(BIN) $(OBJS)
