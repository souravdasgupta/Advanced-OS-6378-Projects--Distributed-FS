CC = g++ # C++ compiler
RM = rm -f    # rm command
TARGET = mserver

DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

SRCS= $(wildcard *.cpp)
DEPS = $(wildcard *.hpp)
OBJS = $(SRCS:.c=.o)

CXXFLAGS = -O3 -o $(TARGET) -rdynamic -D_GNU_SOURCE -L../build/ 
LIBS = 

all:
	$(CC) $(SRCS) $(LIBS) $(CXXFLAGS) -lrt -static-libstdc++

clean:
	rm -f $(TARGET) *.o