/* Replays the transcendental x87 instructions that an instrumented translator recorded on the
 * processor and reports the results that differ. Each input line is
 *
 *     <name> <exponent>:<significand> <exponent>:<significand>
 *
 * with the operand and the result of the translator in hexadecimal, where <name> is fsin, fcos,
 * fptan, fsincos-sin or fsincos-cos. Unique operands are replayed once.
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

static struct x87 run(const char *name, struct x87 in) {
    unsigned char buffer[10];
    unsigned char out[10];
    memcpy(buffer, &in.significand, 8);
    memcpy(buffer + 8, &in.exponent, 2);
    if (strcmp(name, "fsin") == 0 || strcmp(name, "fsincos-sin") == 0) {
        __asm__ volatile("fldt %1\n\tfsin\n\tfstpt %0" : "=m"(out) : "m"(buffer));
    } else if (strcmp(name, "fcos") == 0 || strcmp(name, "fsincos-cos") == 0) {
        __asm__ volatile("fldt %1\n\tfcos\n\tfstpt %0" : "=m"(out) : "m"(buffer));
    } else {
        __asm__ volatile("fldt %1\n\tfptan\n\tfstp %%st(0)\n\tfstpt %0" : "=m"(out) : "m"(buffer));
    }
    struct x87 result;
    memcpy(&result.significand, out, 8);
    memcpy(&result.exponent, out + 8, 2);
    return result;
}

int main(void) {
    char name[32];
    unsigned in_exp, out_exp;
    unsigned long long in_sig, out_sig;
    unsigned long total = 0, differ = 0;
    while (scanf("%31s %x:%llx %x:%llx", name, &in_exp, &in_sig, &out_exp, &out_sig) == 5) {
        struct x87 in = {in_sig, (uint16_t) in_exp};
        struct x87 hardware = run(name, in);
        ++total;
        if (hardware.significand != out_sig || hardware.exponent != out_exp) {
            ++differ;
            long long ulps = (long long) (hardware.significand - out_sig);
            printf("%s %04x:%016llx translator %04x:%016llx processor %04x:%016llx (%+lld)\n",
                   name, in_exp, in_sig, out_exp, out_sig, hardware.exponent,
                   (unsigned long long) hardware.significand, ulps);
        }
    }
    printf("%lu results, %lu differ\n", total, differ);
    return 0;
}
