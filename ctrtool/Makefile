OBJS = keyset.o main.o ctr.o ncsd.o cia.o tik.o tmd.o ncch.o utils.o
POLAR_OBJS = polarssl/aes.o polarssl/bignum.o polarssl/rsa.o polarssl/sha2.o
TINYXML_OBJS = tinyxml/tinystr.o tinyxml/tinyxml.o tinyxml/tinyxmlerror.o tinyxml/tinyxmlparser.o
LIBS = -lstdc++
CXXFLAGS = -I. 
CFLAGS = -Wall -I.
OUTPUT = ctrtool

main: $(OBJS) $(POLAR_OBJS) $(TINYXML_OBJS)
	gcc -o $(OUTPUT) $(LIBS) $(OBJS) $(POLAR_OBJS) $(TINYXML_OBJS)
clean:
	rm -rf $(OUTPUT) $(OBJS) $(POLAR_OBJS) $(TINYXML_OBJS)
