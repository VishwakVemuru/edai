# make                     sensor_run: the brain on windows of sensor readings (TARGET=linux-x86_64 or linux-aarch64 on Linux)
# make hidtemp             the reader of this Mac's chip temperature sensors, for mac_live.py (macOS on Apple silicon)
TARGET ?= macos-arm64
sensor_run: sensor_run.c
	cc -std=c11 -O2 -I../brain/include sensor_run.c ../brain/lib/$(TARGET)/libaa.a -o sensor_run
hidtemp: hidtemp.c
	cc -O2 hidtemp.c -framework IOKit -framework CoreFoundation -o hidtemp
