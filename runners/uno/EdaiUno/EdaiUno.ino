// edai brain, Uno edition (experimental): the brain runs on the Arduino Uno's ATmega328P itself (2 KB of RAM, 16 MHz).
// Needs the EdaiUnoBrain library (copy it into your Arduino libraries folder).
// It answers the 9 problems of edai's quick set and prints one line per problem, the score, the time and how much RAM
// was never touched. Open the Serial Monitor at 115200 baud and leave it running: a problem can take several minutes.
#include <Arduino.h>
#include <avr/pgmspace.h>
#include <edai_uno.h>
#include "problems_uno.h"

// A small serial writer (115200 baud, 8N1, the same settings as Serial) instead of Serial: less code, and no RAM buffers.
static void uartBegin() { UCSR0A = _BV(U2X0); UBRR0 = 16; UCSR0B = _BV(TXEN0); UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); }
static void putC(char c) { if (c == '\n') putC('\r'); while (!(UCSR0A & _BV(UDRE0))) {} UDR0 = c; }
static void putFlash(PGM_P s) { char c; while ((c = pgm_read_byte(s++))) putC(c); }
#define P(s) putFlash(PSTR(s))
static void putU(uint32_t v) { char b[11]; int n = 0; do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) putC(b[--n]); }
static void putI64(int64_t v) {
  char b[21]; int n = 0;
  uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
  do { b[n++] = (char)('0' + (int)(u % 10)); u /= 10; } while (u);
  if (v < 0) putC('-');
  while (n) putC(b[--n]);
}

extern uint8_t __bss_end;                    // the end of the program's own variables: free RAM starts here
static void paintFreeRam() {                 // fill the free RAM with a pattern, to see afterwards how much was used
  volatile uint8_t *p = &__bss_end;
  uint8_t *sp = (uint8_t *)SP;
  while (p < sp - 32) *p++ = 0xA5;
}
static unsigned untouchedRam() {
  volatile uint8_t *p = &__bss_end;
  unsigned n = 0;
  while (p < (uint8_t *)SP && *p == 0xA5) { p++; n++; }
  return n;
}

static bool rightAnswer(const uno_problem &p, const eu_answer &a) {
  if (p.family == 2) return a.by == 0;
  if (a.by == 0) return false;
  if (p.family == 0) return (float)a.value[0] == p.truth[0] && (float)a.value[1] == p.truth[1];
  for (int k = 0; k < 2; k++) { float d = (float)a.value[k] - p.truth[k]; if (d < 0) d = -d; if (d > p.tol) return false; }
  return true;
}

void setup() {
  paintFreeRam();
  uartBegin();
  delay(1500);
  P("EDAI UNO TEST\nBOARD Arduino Uno (ATmega328P, 16 MHz, 2 KB RAM), brain "); putU(eu_brain_bytes());
  P(" bytes (Uno edition)\nSTART "); putU(N_UNO); P(" problems\n");
  int right = 0, famRight[3] = { 0, 0, 0 }, famN[3] = { 0, 0, 0 };
  uint32_t total = 0;
  for (int j = 0; j < N_UNO; j++) {
    uno_problem p;
    memcpy_P(&p, &UNO_PROBLEMS[j], sizeof p);
    eu_answer a;
    uint32_t t0 = millis();
    int rc = eu_solve_rule(p.x, p.y, p.n, p.q, 2, &a);
    uint32_t ms = millis() - t0;
    total += ms;
    bool ok = rc == 0 && rightAnswer(p, a);
    right += ok; famRight[p.family] += ok; famN[p.family]++;
    putC('p'); if (p.id < 100) putC('0'); if (p.id < 10) putC('0'); putU(p.id);
    if (p.family == 0) P(" rule "); else if (p.family == 1) P(" trend"); else P(" noise");
    if (rc) { P(" error\n"); continue; }
    P(" by=");
    if (a.by == 1) P("AM"); else if (a.by == 2) P("SPLITTER"); else if (a.by == 3) P("NET"); else P("CANNOT");
    P(" ans=");
    if (a.by == 0) P("CANNOT"); else { putI64(a.value[0]); putC(','); putI64(a.value[1]); }
    if (a.learned) { P(" check="); putU((uint32_t)a.check); }
    if (ok) P(" right=1"); else P(" right=0");
    P(" ms="); putU(ms); P(".000 cycles=none\n");
    P("RAM never touched so far: "); putU(untouchedRam()); P(" bytes\n");
  }
  P("SUMMARY right "); putU(right); putC('/'); putU(N_UNO);
  P(" rules "); putU(famRight[0]); putC('/'); putU(famN[0]);
  P(" trends "); putU(famRight[1]); putC('/'); putU(famN[1]);
  P(" noise "); putU(famRight[2]); putC('/'); putU(famN[2]);
  P("\nTIME all "); putU(total / 1000); P(" s\nRAM never touched during the run: "); putU(untouchedRam()); P(" bytes\nDONE\n");
}

void loop() {}
