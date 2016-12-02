all: runner
test: runner
	./runner -v

override CXXFLAGS := $(CXXFLAGS) -std=c++0x -I. -Wall -Wextra

CXX_SRC  = runner.cpp testsuite.cpp ../threadpool.cpp ../threadpool_p.cpp ../threadpoolthread.cpp
CXX_DEPS = $(patsubst %.cpp,%.d,$(CXX_SRC))
CXX_OBJ  = $(patsubst %.cpp,%.o,$(CXX_SRC))

ifeq ($(MAKECMDGOALS),coverage)
EXTRA_CXXFLAGS += -O0 --coverage
EXTRA_LDFLAGS  += --coverage
endif

ifneq ($(MAKECMDGOALS),clean)

ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif

endif

clean-coverage:
	-rm -rf .coverage coverage

runner: $(CXX_OBJ)
	$(CXX) $^ $(LDFLAGS) $(EXTRA_LDFLAGS) -pthread -o $@

runner.o: runner.cpp

runner.cpp: testsuite.h
	cxxtestgen --error-printer -o "$@" "$<"

clean: clean-coverage
clean:
	rm -f runner runner.cpp $(CXX_DEPS) $(CXX_OBJ)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EXTRA_CXXFLAGS) -c "$<" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@"

coverage: test clean-coverage
	lcov --quiet --directory ../ --capture --base-directory=../ --output-file .coverage
	lcov --quiet --remove .coverage "/usr/*" -o .coverage
	genhtml --quiet --legend --output-directory coverage/ --title "cpp11-threadpool code coverage" .coverage

.PHONY: clean
