all: threadpool.o threadpool_p.o threadpoolthread.o

clean:
	-rm -f threadpool.o threadpool_p.o threadpoolthread.o

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -std=c++11 -c "$<" -o "$@"

.PHONY: clean
