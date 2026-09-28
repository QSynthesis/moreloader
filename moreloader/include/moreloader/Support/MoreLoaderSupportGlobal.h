#ifndef MORELOADER_SUPPORT_MORELOADERSUPPORTGLOBAL_H
#define MORELOADER_SUPPORT_MORELOADERSUPPORTGLOBAL_H

/// Compiler-specific attributes used throughout the module. Every sub-library includes this
/// header rather than writing attributes directly, so that the sources remain acceptable to MSVC,
/// with which the probes and editors parse them.

#if defined(_MSC_VER) && !defined(__clang__)
#  define MORE_PRINTF_FORMAT(formatIndex, firstArgument)
#  define MORE_PRINTF_FORMAT_STRING _Printf_format_string_
#  define MORE_NOINLINE             __declspec(noinline)
#  define MORE_STDCALL              __stdcall
#  define MORE_CDECL_CONVENTION     __cdecl
#  define MORE_FORCE_ALIGN_STACK
#else
/// Checks the arguments of a printf-style function against its format string.
#  define MORE_PRINTF_FORMAT(formatIndex, firstArgument)                                          \
      __attribute__((format(printf, formatIndex, firstArgument)))
/// Annotates the format string parameter of a printf-style function for MSVC.
#  define MORE_PRINTF_FORMAT_STRING
#  define MORE_NOINLINE __attribute__((noinline))
#  define MORE_STDCALL  __attribute__((stdcall))
#  define MORE_CDECL_CONVENTION __attribute__((cdecl))
/// Realigns the stack on entry. The guest guarantees only 4-byte alignment, while code generated
/// for i386 Linux assumes 16-byte alignment at every call.
#  define MORE_FORCE_ALIGN_STACK __attribute__((force_align_arg_pointer))
#endif

/// Calling convention of a wrapper that emulates a \c __stdcall function of Windows.
#define MORE_WINAPI MORE_STDCALL MORE_FORCE_ALIGN_STACK

/// Calling convention of a wrapper that emulates a \c __cdecl function of msvcrt.
#define MORE_CDECL MORE_CDECL_CONVENTION MORE_FORCE_ALIGN_STACK

#endif // MORELOADER_SUPPORT_MORELOADERSUPPORTGLOBAL_H
