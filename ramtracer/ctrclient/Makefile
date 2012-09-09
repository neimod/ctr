LIBS = 
OBJS = main.o ctrclient.o 
CXXFLAGS = -I. 
CFLAGS = -Wall -I. 
OUTPUT = ctrclient 
CC = gcc 

main: $(OBJS)
	g++ -o $(OUTPUT) $(LIBS) $(OBJS) 
clean:
	rm -rf $(OUTPUT) $(OBJS)
