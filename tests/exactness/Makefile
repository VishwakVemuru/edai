# make                      builds ./answer with the brain for this Mac (Apple silicon) and runs the exactness test
# make TARGET=linux-x86_64  (or linux-aarch64) on Linux
# make LIB=<a libaa.a>      the same test with another build of the brain, such as version 1.0 from the history:
#                           git show a675abd:brain/lib/macos-arm64/libaa.a > /tmp/libaa-1.0.a && make LIB=/tmp/libaa-1.0.a
TARGET ?= macos-arm64
LIB ?= ../../brain/lib/$(TARGET)/libaa.a
.PHONY: check
check: answer
	python3 exactness.py ./answer
answer: answer.c $(LIB)
	cc -std=c11 -O2 -I../../brain/include answer.c $(LIB) -o answer
