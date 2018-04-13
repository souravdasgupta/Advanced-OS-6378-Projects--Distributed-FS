CC = g++ # C++ compiler
RM = rm -f    # rm command
TARGET = server

DEBUGFLAGS   = -O0 -D _DEBUG
RELEASEFLAGS = -O2 -D NDEBUG -combine -fwhole-program

SRCS= $(wildcard *.cpp)
DEPS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

CXXFLAGS = -O3 -o $(TARGET) -rdynamic -D_GNU_SOURCE -L../build/ -lrt -static-libstdc++
LIBS =

all:
	$(CC) $(SRCS) $(LIBS) $(CXXFLAGS) 

clean:
	rm -f $(TARGET) *.o