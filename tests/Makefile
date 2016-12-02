all: runner
test: runner
	./runner -v

CXXFLAGS += -std=c++0x -I.

CXX_SRC  = runner.cpp ../threadpool.cpp ../threadpool_p.cpp ../threadpoolthread.cpp
CXX_DEPS = $(patsubst %.cpp,%.d,$(CXX_SRC))
CXX_OBJ  = $(patsubst %.cpp,%.o,$(CXX_SRC))

ifeq ($(MAKECMDGOALS),coverage)
CXXFLAGS += -O0 --coverage
LDFLAGS  += --coverage
endif

ifneq ($(MAKECMDGOALS),clean)

ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif

endif

clean-coverage:
	-rm -rf .coverage coverage

runner: runner.o ../threadpool.o ../threadpool_p.o ../threadpoolthread.o
	$(CXX) $^ $(LDFLAGS) -pthread -o $@

runner.o: runner.cpp

runner.cpp: testsuite.h
	cxxtestgen --error-printer -o "$@" "$<"

clean: clean-coverage
clean:
	rm -f runner runner.cpp $(CXX_DEPS) $(CXX_OBJ)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c "$<" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@"

coverage: test clean-coverage
	lcov --quiet --directory ../ --capture --base-directory=../ --output-file .coverage
	lcov --quiet --remove .coverage "/usr/*" -o .coverage
	genhtml --quiet --legend --output-directory coverage/ --title "cpp11-threadpool code coverage" .coverage

.PHONY: clean
