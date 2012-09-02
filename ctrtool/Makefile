OBJS = keyset.o main.o ctr.o ncsd.o cia.o tik.o tmd.o filepath.o lzss.o exheader.o exefs.o ncch.o utils.o settings.o firm.o cwav.o stream.o romfs.o ivfc.o
POLAR_OBJS = polarssl/aes.o polarssl/bignum.o polarssl/rsa.o polarssl/sha2.o
TINYXML_OBJS = tinyxml/tinystr.o tinyxml/tinyxml.o tinyxml/tinyxmlerror.o tinyxml/tinyxmlparser.o
LIBS = -lstdc++
CXXFLAGS = -I. 
CFLAGS = -Wall -I.
OUTPUT = ctrtool
CC = gcc

main: $(OBJS) $(POLAR_OBJS) $(TINYXML_OBJS)
	g++ -o $(OUTPUT) $(LIBS) $(OBJS) $(POLAR_OBJS) $(TINYXML_OBJS)


clean:
	rm -rf $(OUTPUT) $(OBJS) $(POLAR_OBJS) $(TINYXML_OBJS)
