UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
        # Default install location on Mac OS
	CFLAGS += -I/opt/local/include
	LDFLAGS += -L/opt/local/lib -lz

	# Large file support
	CFLAGS += -DFILE_OFFSET_BITS=64
else
        # Load libusb via pkg-config
	PACKAGES := 
	CFLAGS += $(shell pkg-config --cflags $(PACKAGES))
	LDFLAGS += $(shell pkg-config --libs $(PACKAGES))

	# Large file support
	CFLAGS += $(shell getconf LFS_CFLAGS)
endif

# Local headers
CFLAGS += -I../include

BIN := ramtracer
OBJS := main.o 

CFLAGS += -O3 -g

all: $(BIN)

$(BIN): $(OBJS)
	cc -o $(BIN) $(OBJS) $(LDFLAGS)

*.o: 

clean:
	rm -f $(BIN) $(OBJS)
