/* Prints this Mac's own temperature sensors (Apple silicon), read the way activity monitors read them: through the
   HID event system, with no administrator rights.   ./hidtemp            one line: name=value for every sensor
                                                       ./hidtemp <ms>       a line every <ms> milliseconds, forever
   cc -O2 hidtemp.c -framework IOKit -framework CoreFoundation -o hidtemp */
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct __IOHIDEvent *IOHIDEventRef;
typedef struct __IOHIDServiceClient *IOHIDServiceClientRef;
IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef allocator);
int IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef client, CFDictionaryRef match);
IOHIDEventRef IOHIDServiceClientCopyEvent(IOHIDServiceClientRef service, int64_t type, int32_t options, int64_t timestamp);
double IOHIDEventGetFloatValue(IOHIDEventRef event, int32_t field);
CFTypeRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service, CFStringRef key);

#define TEMPERATURE_EVENT 15

int main(int argc, char **argv)
{
    int every = argc > 1 ? atoi(argv[1]) : 0;
    IOHIDEventSystemClientRef client = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    int page = 0xff00, usage = 5;
    CFNumberRef np = CFNumberCreate(NULL, kCFNumberIntType, &page), nu = CFNumberCreate(NULL, kCFNumberIntType, &usage);
    const void *keys[2] = { CFSTR("PrimaryUsagePage"), CFSTR("PrimaryUsage") }, *vals[2] = { np, nu };
    CFDictionaryRef match = CFDictionaryCreate(NULL, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOHIDEventSystemClientSetMatching(client, match);
    for (;;) {
        CFArrayRef services = IOHIDEventSystemClientCopyServices(client);
        long n = services ? CFArrayGetCount(services) : 0;
        for (long i = 0; i < n; i++) {
            IOHIDServiceClientRef s = (IOHIDServiceClientRef)CFArrayGetValueAtIndex(services, i);
            CFStringRef name = (CFStringRef)IOHIDServiceClientCopyProperty(s, CFSTR("Product"));
            IOHIDEventRef ev = IOHIDServiceClientCopyEvent(s, TEMPERATURE_EVENT, 0, 0);
            if (name && ev) {
                char buf[128];
                CFStringGetCString(name, buf, sizeof buf, kCFStringEncodingUTF8);
                printf("%s%s=%.2f", i ? "\t" : "", buf, IOHIDEventGetFloatValue(ev, TEMPERATURE_EVENT << 16));
            }
            if (ev) CFRelease(ev);
            if (name) CFRelease(name);
        }
        printf("\n");
        fflush(stdout);
        if (services) CFRelease(services);
        if (!every) break;
        usleep((useconds_t)every * 1000);
    }
    return 0;
}
