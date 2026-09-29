#ifndef MORELOADER_RUNTIME_GUESTLAYOUT_H
#define MORELOADER_RUNTIME_GUESTLAYOUT_H

#include <cstddef>
#include <cstdint>

/// Memory layouts of Windows structures that the guest reads, in their 32-bit form. Addresses
/// are stored as 32-bit integers, because the guest shares the address space of the host.
/// Field names follow the Windows SDK. Only the fields that the loader maintains are named.

namespace more::loader {

    /// Thread environment block, addressed through FS.
    struct TEB32 {
        std::uint32_t ExceptionList;             // 0x000
        std::uint32_t StackBase;                 // 0x004
        std::uint32_t StackLimit;                // 0x008
        std::uint32_t SubSystemTib;              // 0x00C
        std::uint32_t FiberData;                 // 0x010
        std::uint32_t ArbitraryUserPointer;      // 0x014
        std::uint32_t Self;                      // 0x018
        std::uint32_t EnvironmentPointer;        // 0x01C
        std::uint32_t UniqueProcess;             // 0x020
        std::uint32_t UniqueThread;              // 0x024
        std::uint32_t ActiveRpcHandle;           // 0x028
        std::uint32_t ThreadLocalStoragePointer; // 0x02C
        std::uint32_t ProcessEnvironmentBlock;   // 0x030
        std::uint32_t LastErrorValue;            // 0x034
        std::uint8_t Reserved1[0xE10 - 0x038];
        std::uint32_t TlsSlots[64];              // 0xE10
        std::uint8_t Reserved2[0xF94 - 0xF10];
        std::uint32_t TlsExpansionSlots;         // 0xF94
        std::uint8_t Reserved3[0x1000 - 0xF98];
    };
    static_assert(sizeof(TEB32) == 0x1000);
    static_assert(offsetof(TEB32, Self) == 0x18);
    static_assert(offsetof(TEB32, ThreadLocalStoragePointer) == 0x2C);
    static_assert(offsetof(TEB32, LastErrorValue) == 0x34);
    static_assert(offsetof(TEB32, TlsSlots) == 0xE10);
    static_assert(offsetof(TEB32, TlsExpansionSlots) == 0xF94);

    /// Process environment block. The guest does not read it, but the TEB refers to it.
    struct PEB32 {
        std::uint8_t InheritedAddressSpace;  // 0x000
        std::uint8_t ReadImageFileExecOptions; // 0x001
        std::uint8_t BeingDebugged;          // 0x002
        std::uint8_t BitField;               // 0x003
        std::uint32_t Mutant;                // 0x004
        std::uint32_t ImageBaseAddress;      // 0x008
        std::uint8_t Reserved[0x1000 - 0x00C];
    };
    static_assert(sizeof(PEB32) == 0x1000);

    /// Number of TLS slots in the TEB. Expansion slots are not provided.
    constexpr std::uint32_t tlsMinimumAvailable = 64;

    /// Reasons passed to TLS callbacks.
    enum DLLReason : std::uint32_t {
        DLLProcessDetach = 0,
        DLLProcessAttach = 1,
        DLLThreadAttach = 2,
        DLLThreadDetach = 3,
    };

    /// \c CRITICAL_SECTION. The loader stores its own lock in \a LockSemaphore.
    struct CRITICAL_SECTION32 {
        std::uint32_t DebugInfo;
        std::int32_t LockCount;
        std::int32_t RecursionCount;
        std::uint32_t OwningThread;
        std::uint32_t LockSemaphore;
        std::uint32_t SpinCount;
    };
    static_assert(sizeof(CRITICAL_SECTION32) == 24);

    /// \c MEMORY_BASIC_INFORMATION.
    struct MEMORY_BASIC_INFORMATION32 {
        std::uint32_t BaseAddress;
        std::uint32_t AllocationBase;
        std::uint32_t AllocationProtect;
        std::uint32_t RegionSize;
        std::uint32_t State;
        std::uint32_t Protect;
        std::uint32_t Type;
    };
    static_assert(sizeof(MEMORY_BASIC_INFORMATION32) == 28);

    /// \c FILETIME, 100-nanosecond intervals since 1601-01-01.
    struct FILETIME32 {
        std::uint32_t dwLowDateTime;
        std::uint32_t dwHighDateTime;
    };

    /// \c WIN32_FIND_DATAW.
    struct WIN32_FIND_DATAW32 {
        std::uint32_t dwFileAttributes;
        FILETIME32 ftCreationTime;
        FILETIME32 ftLastAccessTime;
        FILETIME32 ftLastWriteTime;
        std::uint32_t nFileSizeHigh;
        std::uint32_t nFileSizeLow;
        std::uint32_t dwReserved0;
        std::uint32_t dwReserved1;
        char16_t cFileName[260];
        char16_t cAlternateFileName[14];
    };
    static_assert(sizeof(WIN32_FIND_DATAW32) == 592);
    static_assert(offsetof(WIN32_FIND_DATAW32, cFileName) == 44);

    /// \c WIN32_FIND_DATAA.
    struct WIN32_FIND_DATAA32 {
        std::uint32_t dwFileAttributes;
        FILETIME32 ftCreationTime;
        FILETIME32 ftLastAccessTime;
        FILETIME32 ftLastWriteTime;
        std::uint32_t nFileSizeHigh;
        std::uint32_t nFileSizeLow;
        std::uint32_t dwReserved0;
        std::uint32_t dwReserved1;
        char cFileName[260];
        char cAlternateFileName[14];
    };
    static_assert(sizeof(WIN32_FIND_DATAA32) == 320);
    static_assert(offsetof(WIN32_FIND_DATAA32, cFileName) == 44);

    /// \c SYSTEMTIME.
    struct SYSTEMTIME32 {
        std::uint16_t wYear;
        std::uint16_t wMonth;
        std::uint16_t wDayOfWeek;
        std::uint16_t wDay;
        std::uint16_t wHour;
        std::uint16_t wMinute;
        std::uint16_t wSecond;
        std::uint16_t wMilliseconds;
    };
    static_assert(sizeof(SYSTEMTIME32) == 16);

    /// \c TIME_ZONE_INFORMATION.
    struct TIME_ZONE_INFORMATION32 {
        std::int32_t Bias;
        char16_t StandardName[32];
        SYSTEMTIME32 StandardDate;
        std::int32_t StandardBias;
        char16_t DaylightName[32];
        SYSTEMTIME32 DaylightDate;
        std::int32_t DaylightBias;
    };
    static_assert(sizeof(TIME_ZONE_INFORMATION32) == 172);

    /// \c OVERLAPPED.
    struct OVERLAPPED32 {
        std::uint32_t Internal;
        std::uint32_t InternalHigh;
        std::uint32_t Offset;
        std::uint32_t OffsetHigh;
        std::uint32_t hEvent;
    };
    static_assert(sizeof(OVERLAPPED32) == 20);

    /// \c STARTUPINFOA.
    struct STARTUPINFOA32 {
        std::uint32_t cb;
        std::uint8_t Reserved[64];
    };
    static_assert(sizeof(STARTUPINFOA32) == 68);

    /// \c OSVERSIONINFOA. \c OSVERSIONINFOEXA extends it to 156 bytes.
    struct OSVERSIONINFOA32 {
        std::uint32_t dwOSVersionInfoSize;
        std::uint32_t dwMajorVersion;
        std::uint32_t dwMinorVersion;
        std::uint32_t dwBuildNumber;
        std::uint32_t dwPlatformId;
        char szCSDVersion[128];
    };
    static_assert(sizeof(OSVERSIONINFOA32) == 148);

    /// \c CPINFO.
    struct CPINFO32 {
        std::uint32_t MaxCharSize;
        std::uint8_t DefaultChar[2];
        std::uint8_t LeadByte[12];
    };
    static_assert(sizeof(CPINFO32) == 20);
}

#endif // MORELOADER_RUNTIME_GUESTLAYOUT_H
