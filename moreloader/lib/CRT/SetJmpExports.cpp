#include "CRTExports_p.h"

#if !defined(__GNUC__) || !defined(__i386__)
#  error "_setjmp3 and longjmp are implemented in i386 assembly for GCC and Clang."
#endif

// _setjmp3 and longjmp of msvcrt operate on the _JUMP_BUFFER of the guest: Ebp at 0, Ebx at 4,
// Edi at 8, Esi at 12, Esp at 16, Eip at 20, Registration at 24, TryLevel at 28, Cookie at 32
// and UnwindFunc at 36. The guest uses both only on its own stack, and it has no structured
// exception handling frames to unwind, therefore longjmp restores the registers and jumps.
//
// _setjmp3 is cdecl and variadic. The saved Esp is the value after the return, which removes the
// return address but not the arguments, because the caller removes those.

extern "C" void moreloader_msvcrt__setjmp3();
extern "C" void moreloader_msvcrt_longjmp();

asm(R"(
    .text
    .globl moreloader_msvcrt__setjmp3
    .type moreloader_msvcrt__setjmp3, @function
moreloader_msvcrt__setjmp3:
    movl 4(%esp), %edx
    movl %ebp, 0(%edx)
    movl %ebx, 4(%edx)
    movl %edi, 8(%edx)
    movl %esi, 12(%edx)
    leal 4(%esp), %eax
    movl %eax, 16(%edx)
    movl (%esp), %eax
    movl %eax, 20(%edx)
    movl %fs:0, %eax
    movl %eax, 24(%edx)
    movl $-1, 28(%edx)
    movl $0x56433230, 32(%edx)
    movl $0, 36(%edx)
    xorl %eax, %eax
    ret
    .size moreloader_msvcrt__setjmp3, .-moreloader_msvcrt__setjmp3

    .globl moreloader_msvcrt_longjmp
    .type moreloader_msvcrt_longjmp, @function
moreloader_msvcrt_longjmp:
    movl 4(%esp), %edx
    movl 8(%esp), %eax
    testl %eax, %eax
    jnz 1f
    incl %eax
1:
    movl 0(%edx), %ebp
    movl 4(%edx), %ebx
    movl 8(%edx), %edi
    movl 12(%edx), %esi
    movl 16(%edx), %esp
    jmp *20(%edx)
    .size moreloader_msvcrt_longjmp, .-moreloader_msvcrt_longjmp
)");

namespace more::loader::msvcrt {

    void registerSetJmpExports(ExportRegistry &registry) {
        registry.add("msvcrt", "_setjmp3", reinterpret_cast<void *>(&moreloader_msvcrt__setjmp3));
        registry.add("msvcrt", "longjmp", reinterpret_cast<void *>(&moreloader_msvcrt_longjmp));
    }

}
