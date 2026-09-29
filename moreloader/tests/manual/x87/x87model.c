/* Computes fsin, fcos and fptan by a model of the processor: reduction of the operand by a multiple
 * of pi/2 rounded to 66 bits, with one rounding in binary128, evaluation in binary128 and rounding
 * to the 80-bit format. Each input line is
 *
 *     <name> <exponent>:<significand>
 *
 * with <name> fsin, fcos, fptan, fsincos-sin or fsincos-cos. Each output line appends the result
 * of the model in the same form, which is the input of x87replay. x87replay then reports the
 * operands for which the processor differs from the model.
 *
 *     gcc -O2 x87model.c -lquadmath -o x87model
 *     x87model < operands | x87replay
 *
 * The program runs on x86_64, where long double is the 80-bit format. */
#include <quadmath.h>
#include <stdio.h>
#include <string.h>

static __float128 fromX87(unsigned exponent, unsigned long long significand) {
    long double value;
    unsigned char bytes[sizeof value];
    unsigned short e = (unsigned short) exponent;
    memset(bytes, 0, sizeof bytes);
    memcpy(bytes, &significand, 8);
    memcpy(bytes + 8, &e, 2);
    memcpy(&value, bytes, sizeof value);
    return (__float128) value;
}

static void toX87(__float128 q, unsigned *exponent, unsigned long long *significand) {
    long double value = (long double) q;
    unsigned char bytes[sizeof value];
    unsigned short e;
    memcpy(bytes, &value, sizeof value);
    memcpy(significand, bytes, 8);
    memcpy(&e, bytes + 8, 2);
    *exponent = e;
}

int main(void) {
    /* pi/2 rounded to 66 bits: the significand c90fdaa22168c234 followed by the bits 11. */
    const __float128 halfPi =
        ldexpq((__float128) 0xc90fdaa22168c234ULL, -63) + ldexpq((__float128) 3, -65);
    char line[128];
    char name[32];
    unsigned exponent;
    unsigned long long significand;
    while (fgets(line, sizeof line, stdin)) {
        if (sscanf(line, "%31s %x:%llx", name, &exponent, &significand) != 3) {
            continue;
        }
        __float128 x = fromX87(exponent, significand);
        __float128 k = rintq(x / halfPi);
        __float128 r = fmaq(-k, halfPi, x);
        long long quadrant = ((long long) k) & 3;
        __float128 s = sinq(r), c = cosq(r), value;
        if (strcmp(name, "fsin") == 0 || strcmp(name, "fsincos-sin") == 0) {
            value = quadrant == 0 ? s : quadrant == 1 ? c : quadrant == 2 ? -s : -c;
        } else if (strcmp(name, "fcos") == 0 || strcmp(name, "fsincos-cos") == 0) {
            value = quadrant == 0 ? c : quadrant == 1 ? -s : quadrant == 2 ? -c : s;
        } else {
            value = (quadrant & 1) ? -c / s : tanq(r);
        }
        unsigned resultExponent;
        unsigned long long resultSignificand;
        toX87(value, &resultExponent, &resultSignificand);
        printf("%s %04x:%016llx %04x:%016llx\n", name, exponent, significand, resultExponent,
               resultSignificand);
    }
    return 0;
}
