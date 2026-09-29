/* Replays the non-exact x87 instructions that an instrumented translator recorded on the processor
 * and reports the results that differ. Each input line is one of
 *
 *     <name> <exponent>:<significand> <exponent>:<significand>
 *     <name> <exponent>:<significand> <exponent>:<significand> <exponent>:<significand>
 *
 * in hexadecimal. The first form records ST0 and the result of the translator for fsin, fcos,
 * fptan, fsincos-sin, fsincos-cos and f2xm1. The second form records ST0, ST1 and the result for
 * fyl2x, fyl2xp1 and fpatan.
 *
 *     gcc -m32 -O0 -static x87replay.c -o x87replay
 *     x87replay < log */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct x87 {
    uint64_t significand;
    uint16_t exponent;
};

static void store(unsigned char *buffer, struct x87 value) {
    memcpy(buffer, &value.significand, 8);
    memcpy(buffer + 8, &value.exponent, 2);
}

static struct x87 run(const char *name, struct x87 st0, struct x87 st1) {
    unsigned char a[10];
    unsigned char b[10];
    unsigned char out[10];
    store(a, st0);
    store(b, st1);
    if (strcmp(name, "fsin") == 0 || strcmp(name, "fsincos-sin") == 0) {
        __asm__ volatile("fldt %1\n\tfsin\n\tfstpt %0" : "=m"(out) : "m"(a));
    } else if (strcmp(name, "fcos") == 0 || strcmp(name, "fsincos-cos") == 0) {
        __asm__ volatile("fldt %1\n\tfcos\n\tfstpt %0" : "=m"(out) : "m"(a));
    } else if (strcmp(name, "fptan") == 0) {
        __asm__ volatile("fldt %1\n\tfptan\n\tfstp %%st(0)\n\tfstpt %0" : "=m"(out) : "m"(a));
    } else if (strcmp(name, "f2xm1") == 0) {
        __asm__ volatile("fldt %1\n\tf2xm1\n\tfstpt %0" : "=m"(out) : "m"(a));
    } else if (strcmp(name, "fyl2x") == 0) {
        __asm__ volatile("fldt %2\n\tfldt %1\n\tfyl2x\n\tfstpt %0" : "=m"(out) : "m"(a), "m"(b));
    } else if (strcmp(name, "fyl2xp1") == 0) {
        __asm__ volatile("fldt %2\n\tfldt %1\n\tfyl2xp1\n\tfstpt %0" : "=m"(out) : "m"(a), "m"(b));
    } else {
        __asm__ volatile("fldt %2\n\tfldt %1\n\tfpatan\n\tfstpt %0" : "=m"(out) : "m"(a), "m"(b));
    }
    struct x87 result;
    memcpy(&result.significand, out, 8);
    memcpy(&result.exponent, out + 8, 2);
    return result;
}

int main(void) {
    char line[256];
    char name[32];
    unsigned e0, e1, e2;
    unsigned long long s0, s1, s2;
    unsigned long total = 0, differ = 0;
    while (fgets(line, sizeof line, stdin)) {
        struct x87 st0, st1 = {0, 0}, recorded;
        int fields =
            sscanf(line, "%31s %x:%llx %x:%llx %x:%llx", name, &e0, &s0, &e1, &s1, &e2, &s2);
        if (fields == 7) {
            st0 = (struct x87) {s0, (uint16_t) e0};
            st1 = (struct x87) {s1, (uint16_t) e1};
            recorded = (struct x87) {s2, (uint16_t) e2};
        } else if (fields == 5) {
            st0 = (struct x87) {s0, (uint16_t) e0};
            recorded = (struct x87) {s1, (uint16_t) e1};
        } else {
            continue;
        }
        struct x87 hardware = run(name, st0, st1);
        ++total;
        if (hardware.significand != recorded.significand ||
            hardware.exponent != recorded.exponent) {
            ++differ;
            long long ulps = (long long) (hardware.significand - recorded.significand);
            line[strcspn(line, "\n")] = '\0';
            printf("%s processor %04x:%016llx (%+lld)\n", line, hardware.exponent,
                   (unsigned long long) hardware.significand, ulps);
        }
    }
    printf("%lu results, %lu differ\n", total, differ);
    return 0;
}
