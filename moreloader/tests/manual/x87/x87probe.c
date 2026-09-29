/* Prints the bit patterns of x87 results for a set of instructions and inputs, under the control
 * words of Windows (0x27F, 53-bit precision) and Linux (0x37F, 64-bit precision), so that the
 * results of a translator can be compared with those of the processor. The output of a run on
 * the processor and of a run under the translator are compared line by line.
 *
 *     gcc -m32 -O0 -static x87probe.c -o x87probe
 *
 * Without multilib, the flags of cmake/toolchains/linux-i386.cmake select the i386 overlay.
 * box32 of box64 requires a dynamically linked build. See docs/claude/20260929-riscv.md. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void setcw(unsigned short cw) {
    __asm__ volatile("fldcw %0" : : "m"(cw));
}

static void print(const char *name, unsigned short cw, double x, long double r) {
    unsigned char b[10];
    memcpy(b, &r, 10);
    double d = (double) r;
    uint64_t bits;
    memcpy(&bits, &d, 8);
    printf("%-8s cw=%03x x=%-12.6g ext=%02x%02x:%02x%02x%02x%02x%02x%02x%02x%02x dbl=%016llx\n",
           name, cw, x, b[9], b[8], b[7], b[6], b[5], b[4], b[3], b[2], b[1], b[0],
           (unsigned long long) bits);
}

#define UNARY(name, insn)                                                                          \
    static long double name(long double x) {                                                       \
        long double r;                                                                             \
        __asm__ volatile(insn : "=t"(r) : "0"(x));                                                 \
        return r;                                                                                  \
    }
UNARY(op_fsin, "fsin")
UNARY(op_fcos, "fcos")
UNARY(op_fsqrt, "fsqrt")
UNARY(op_f2xm1, "f2xm1")
UNARY(op_frndint, "frndint")

static long double op_fptan(long double x) {
    long double r;
    __asm__ volatile("fptan\n\tfstp %%st(0)" : "=t"(r) : "0"(x));
    return r;
}
static long double op_fyl2x(long double x) { /* log2(x) * 1 */
    long double r;
    __asm__ volatile("fld1\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
    return r;
}
static long double op_fpatan(long double x) { /* atan(x / 1) */
    long double r;
    __asm__ volatile("fld1\n\tfpatan" : "=t"(r) : "0"(x));
    return r;
}
static long double op_div3(long double x) {
    long double three = 3.0L;
    return x / three;
}
static long double op_mulpi(long double x) {
    long double pi = 3.14159265358979323846264338327950288L;
    return x * pi;
}
static long double op_fistp_trunc(long double x) {
    /* The conversion sequence of GCC: round toward zero by the control word, then fistp. */
    unsigned short old, t;
    int32_t i;
    __asm__ volatile("fnstcw %0" : "=m"(old));
    t = old | 0x0C00;
    __asm__ volatile("fldcw %1\n\tfistpl %0\n\tfldcw %2"
                     : "=m"(i)
                     : "m"(t), "m"(old), "t"(x)
                     : "st");
    return (long double) i;
}

int main(void) {
    const double inputs[] = {0.1, 0.5, 0.7, 1.0 / 3.0, 2.5, -2.5, 3.7, 100.3, 1e-5, 12345.678};
    const unsigned short cws[] = {0x27F, 0x37F};
    struct {
        const char *name;
        long double (*f)(long double);
    } ops[] = {
        {"fsin",    op_fsin       },
        {"fcos",    op_fcos       },
        {"fptan",   op_fptan      },
        {"fsqrt",   op_fsqrt      },
        {"f2xm1",   op_f2xm1      },
        {"fyl2x",   op_fyl2x      },
        {"fpatan",  op_fpatan     },
        {"frndint", op_frndint    },
        {"div3",    op_div3       },
        {"mulpi",   op_mulpi      },
        {"fistp",   op_fistp_trunc},
    };
    for (unsigned c = 0; c < 2; ++c) {
        for (unsigned o = 0; o < sizeof ops / sizeof ops[0]; ++o) {
            for (unsigned i = 0; i < sizeof inputs / sizeof inputs[0]; ++i) {
                double x = inputs[i];
                if (ops[o].f == op_f2xm1 && (x > 1 || x < -1))
                    continue;
                if (ops[o].f == op_fyl2x && x <= 0)
                    continue;
                setcw(cws[c]);
                long double r = ops[o].f((long double) x);
                setcw(0x37F);
                print(ops[o].name, cws[c], x, r);
            }
        }
    }
    return 0;
}
