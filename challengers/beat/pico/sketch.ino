// The beat solver on a Raspberry Pi Pico 2 (Arduino core), text to UART0 (Wokwi's serial monitor).
// QUICK 1: the 9 problems of edai's quick set (same problems, so the times compare). QUICK 0: all 300.
// The device holds no answers: copy the log and score it on a computer with score.py.
#define QUICK 1
extern "C" {
#include "beat.h"
#include "problems.h"
int bt_solve(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out);
}
static const char *QUICK_IDS[] = {"p000", "p001", "p002", "p095", "p099", "p105", "p193", "p199", "p213"};

static void put64(int64_t v) { char b[24]; int i = 23; b[i] = 0; uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
  do { b[--i] = '0' + (char)(u % 10); u /= 10; } while (u); if (v < 0) b[--i] = '-'; Serial1.print(&b[i]); }
static void cents(int64_t v) { if (v < 0) { Serial1.print('-'); v = -v; } put64(v / 100); Serial1.print('.');
  if (v % 100 < 10) Serial1.print('0'); put64(v % 100); }

static void run_one(int i, uint64_t *total)
{
  const bt_problem *p = &BT_PROBLEMS[i];
  int64_t x[16], y[16], q[2]; bt_answer a;
  for (int j = 0; j < p->n; j++) { x[j] = p->x[j]; y[j] = p->y[j]; }
  q[0] = p->q[0]; q[1] = p->q[1];
  uint32_t t0 = micros();
  int rc = p->seq ? bt_solve_sequence(x, p->n, &a) : bt_solve(x, y, p->n, q, p->nq, &a);
  uint32_t us = micros() - t0; *total += us;
  static const char *BY[] = {"CANNOT", "RULE", "PIECES", "SEQ", "TREND"};
  Serial1.print(p->id); Serial1.print(' ');
  if (rc) Serial1.print("ERROR");
  else if (a.by == BT_CANNOT) Serial1.print("CANNOT");
  else if (a.by == BT_TREND) { cents(a.value[0]); Serial1.print(','); cents(a.value[1]); }
  else { put64(a.value[0]); Serial1.print(','); put64(a.value[1]); }
  Serial1.print("  # "); Serial1.print(BY[a.by]); Serial1.print(" ms="); Serial1.print(us / 1000); Serial1.print('.');
  Serial1.println((us % 1000) / 100);
}

void setup() {
  Serial1.begin(115200);
  delay(300);
  Serial1.println("BEAT PICO TEST");
  Serial1.print("STATIC RAM "); Serial1.print(bt_ram_bytes()); Serial1.println(" bytes");
  uint64_t total = 0; int count = 0;
  for (int i = 0; i < BT_NPROBLEMS; i++) {
    if (QUICK) { int hit = 0; for (int k = 0; k < 9; k++) if (!strcmp(BT_PROBLEMS[i].id, QUICK_IDS[k])) hit = 1; if (!hit) continue; }
    run_one(i, &total); count++;
  }
  Serial1.print("DONE "); Serial1.print(count); Serial1.print(" problems in "); Serial1.print((uint32_t)(total / 1000)); Serial1.println(" ms (simulator time)");
}
void loop() { delay(1); }
