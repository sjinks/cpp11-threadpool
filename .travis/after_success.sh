#!/bin/bash

if [ "$CC" = "gcc" ]; then
	GCOV=gcov
	ARGS=""
else
	GCOV=llvm-cov
	ARGS=gcov
fi

(cd tests && $GCOV $ARGS *.cpp && /bin/bash <(curl -s https://codecov.io/bash) -X gcov -s .)
