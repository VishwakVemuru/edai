# make                     builds run300 with the brain for this Mac (Apple silicon)
# make TARGET=linux-x86_64 or TARGET=linux-aarch64 on Linux
# make check               runs it, scores it, and compares every line with the reference (expected.txt)
# make fresh               the same brain on the five fresh sets (fresh/seed-NN, problems in ../../problems/fresh):
#                          each one scored with its own key and compared line by line with its reference
SHELL := /bin/bash
.PHONY: check fresh
TARGET ?= macos-arm64
run300: run300.c harness.c harness.h stream_data.h
	cc -std=c11 -O2 -I../../brain/include -I. run300.c harness.c ../../brain/lib/$(TARGET)/libaa.a -o run300
check: run300
	./run300 > my-run.txt
	python3 ../../score.py my-run.txt
	@if diff -q <(grep '^p' expected.txt) <(grep '^p' my-run.txt) >/dev/null; then echo "every line the same as the reference: 300 of 300"; else echo "lines differ from the reference:"; diff <(grep '^p' expected.txt) <(grep '^p' my-run.txt) | head; fi
SETS ?= 23 24 25 26 27
fresh:
	@for s in $(SETS); do d=build-fresh/$$s; mkdir -p $$d; cp run300.c harness.c harness.h fresh/seed-$$s/stream_data.h $$d/; \
	  cc -std=c11 -O2 -I../../brain/include $$d/run300.c $$d/harness.c ../../brain/lib/$(TARGET)/libaa.a -o $$d/run300 || exit 1; \
	  $$d/run300 > $$d/my-run.txt; printf "seed $$s: "; python3 ../../score.py $$d/my-run.txt --key ../../problems/fresh/seed-$$s/answers.json; \
	  if diff -q <(grep '^p' fresh/seed-$$s/expected.txt) <(grep '^p' $$d/my-run.txt) >/dev/null; then echo "         every line the same as the reference: 300 of 300"; \
	  else echo "         lines differ from the reference"; fi; done
