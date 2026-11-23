# make                      runs the light test with the brain for this Mac (Apple silicon)
# make TARGET=linux-x86_64  (or linux-aarch64) on Linux
# make LIB=<a libaa.a>      the same test with another build of the brain, such as version 1.1 from the history:
#                           git show e75b0a3:brain/lib/macos-arm64/libaa.a > /tmp/libaa-1.1.a && make LIB=/tmp/libaa-1.1.a
TARGET ?= macos-arm64
LIB ?= ../../brain/lib/$(TARGET)/libaa.a
.PHONY: check
check: answer
	python3 light.py ./answer
answer: ../exactness/answer.c $(LIB)
	cc -std=c11 -O2 -I../../brain/include ../exactness/answer.c $(LIB) -o answer
