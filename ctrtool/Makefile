OBJS = main.o aes.o ctr.o cia.o tik.o tmd.o ncch.o utils.o
LIBS = 
CFLAGS = -Wall
OUTPUT = ctrtool

main: $(OBJS)
	gcc -o $(OUTPUT) $(LIBS) $(OBJS)
clean:
	rm -rf $(OUTPUT) $(OBJS)
