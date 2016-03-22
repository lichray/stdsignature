# ccconf testit CXX=g++ CXXFLAGS=-std=c++1y -Wall -g -I. -Wsign-promo
CXXFLAGS = -std=c++1y -Wall -g -I. -Wsign-promo
CXX      = g++

.PHONY : all clean
all : testit
clean :
	rm -f testit testit.o tags

tags : *.h testit.cc
	ctags *.h testit.cc

testit : testit.o
	${CXX} ${LDFLAGS} -o testit testit.o
testit.o: testit.cc functional.h
