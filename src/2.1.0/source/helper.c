/*
 * YumePatch: YumeKey Patch Helper 2.0
 *
 * Copyright (C) 2023 Xi Jinpwned Software
 *
 * This software is made available to you under the terms of the GNU Affero
 * General Public License version 3.0. For more information, see the included
 * LICENSE.txt file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>

#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <wchar.h>
#include <intrin.h>
#include "minhook/include/MinHook.h"
#else
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#if defined(__APPLE__) && defined(__arm64__)
#include "fishhook/fishhook.h"
#endif
#endif

#include "helper.h"

#ifdef EXTRA_CODE
EXTRA_CODE
#endif

/* #define DEBUG */

#define SIG0 0x9a79e715
#define SIG1 0x1741776d
#define SIG2 0x5eba54e1
#define SIG3 0xdc747c38
#define SIG(n) SIG##n

#define KEY_SIZE 0x100

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

static _Noreturn void die();

static uint8 hatoi8(char c) {
  return c >= '0' && c <= '9' ? c - '0' :
         c >= 'a' && c <= 'f' ? c - 'a' + 10 :
         c >= 'A' && c <= 'F' ? c - 'A' + 10 :
         (die(), 0);
}

static uint8 _newPubKey[KEY_SIZE] = { 0x0 };
static const uint8* getPubKey() {
  uint8* newPubKey = _newPubKey;

  if (newPubKey[0] != 0x0) {
    return newPubKey;
  }

  const char* newPubKeyStr = getenv("SVPATCH_PUBKEY");
  if (!newPubKeyStr) return false;

  const char* start = strchr(newPubKeyStr, ',');
  if (!start) {
    start = newPubKeyStr;
  } else {
    if (strncmp(newPubKeyStr, "10001,", 6) != 0) {
      return NULL;
    }
    start++;
  }

  int len = strlen(start);
  if (len != 512) {
    return NULL;
  }

  /* This weird loop ensures we store the data in little endian order */
  for (int i = 0, j = 255; i < 256 && j >= 0; i++, j--) {
    newPubKey[j] = hatoi8(start[i * 2 + 1]) | (hatoi8(start[i * 2]) << 4);
  }

  return newPubKey;
}

#if defined(_WIN32) || defined(WIN32)
static const uint64 MEMCPY_SIG[] = {
  0x0031bbe0, 0x4cc18b48, 0x4416158d, /* 1.8.1   */
  0x00326f80, 0x4cc18b48, 0x9076158d, /* 1.9.0b1 */
  0x0032fa60, 0x4cc18b48, 0x0596158d, /* 1.9.0b2 */
  0x0032c230, 0x4cc18b48, 0x3dc6158d, /* 1.9.0   */
  /***********************************************/
  0x002b5770, 0x4cc18b48, 0xa886158d, /* VP1.2.2 */
  0x002b5770, 0x4cc18b48, 0xa886158d, /* VP1.2.3 */
};

static const uint64 MEMCPY_STK[] = {
  0x00000000,                         /* 1.8.1   */
  0x00000001, 0x00000007, 0x000e25f8, /* 1.9.0b1 */
  0x00000001, 0x00000007, 0x000e9288, /* 1.9.0b2 */
  0x00000001, 0x00000007, 0x000e8298, /* 1.9.0   */
  /***********************************************/
  0x00000000,                         /* VP1.2.2 */
  0x00000000,                         /* VP1.2.3 */
};

typedef LPVOID (*api_MemCpy)(LPVOID, LPCVOID, SIZE_T);
typedef WINAPI BOOL (*api_SHGetSpecialFolderPathW)(HWND, LPWSTR, int, BOOL);
typedef WINAPI BOOL (*api_GetVolumeInformationW)(
        LPCWSTR, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD);
typedef WINAPI HANDLE (*api_CreateFileW)(
        LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

static LPCWSTR detourDir;
static bool detourCommon;

static LPCWSTR _lifeCycle = L"UNKNOWN";
#define LIFECYCLE(x) (_lifeCycle = L##x)

static bool inDllMain = false;
static DWORD injectTime = 0;

static inline void dwprintf(LPCWSTR format, ...) {
#ifdef DEBUG
  WCHAR buf[512];
  va_list arg_ptr;

  va_start(arg_ptr, format);
  vswprintf(buf, sizeof(buf) / sizeof(WCHAR), format, arg_ptr);
  va_end(arg_ptr);

  OutputDebugStringW(buf);
#else
  (void)format;
#endif
}

static LPCWSTR getStackTrace() {
  static WCHAR traceBuf[4096] = {0};
  HANDLE base = GetModuleHandleW(NULL);

#if defined(BACKTRACE_GCC)
#define LVL(n) if (i == n) { frame = __builtin_frame_address(n); \
                             func = __builtin_return_address(n); }
  int o = 0;
  for (int i = 1; i <= 16; i++) {
    void *frame = NULL, *func = NULL;
    /* These builtins demand a constant integer parameter, thus this
       abomination was born. */
    LVL(0); LVL(1); LVL(2); LVL(3); LVL(4); LVL(5); LVL(6); LVL(7); LVL(8);
    LVL(9); LVL(10); LVL(11); LVL(2); LVL(13); LVL(14); LVL(15); LVL(16);
    if (!frame || !func || (uint64)func < 0xffffff) break;

    int n = swprintf(traceBuf + o, sizeof(traceBuf) / sizeof(WCHAR) - o,
                     L"[%d] %p (rebase exe @ 0x1000: %p) @ frame %p\n",
                     i - 1, func, (func - base + 0x1000), frame);
    dwprintf(L"SVPatch: getStackTrace(): %S", traceBuf + o);
    o += n;
  }
#undef LVL
#else
  LPVOID stack[16];
  uint16 frames = RtlCaptureStackBackTrace(1, 16, stack, NULL);
  int o = 0;
  for (int i = 0; i < frames; i++) {
    void *func = stack[i];
    int n = swprintf(traceBuf + o, sizeof(traceBuf) / sizeof(WCHAR) - o,
                     L"[%d] %p (rebase exe @ 0x1000: %p)\n",
                     i, func, (func - base + 0x1000));
    dwprintf(L"SVPatch: getStackTrace(): %S", traceBuf + o);
    o += n;
  }
#endif
  return traceBuf;
}

static _Noreturn void die() {
  static WCHAR buffer[1024];
  DWORD lastError = GetLastError();
  LPCWSTR extraError = L"None";

  OSVERSIONINFOEXW osInfo = {};
  NTSTATUS (WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW) = NULL;

  if (!inDllMain) {
    RtlGetVersion = (NTSTATUS(*)(LPOSVERSIONINFOEXW))
                    GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");
  }

  if (RtlGetVersion == NULL) {
    extraError = L"SVPATCH_NO_RTLGETVERSION";
  } else {
    if (RtlGetVersion(&osInfo) != 0) {
      extraError = L"SVPATCH_RTLGETVERSION_FAIL";
    }
  }

  DWORD failTime = GetTickCount();

  swprintf(buffer, sizeof(buffer) / sizeof(WCHAR),
                   L"An internal error has occurred during initialization.\r\n"
                    "\r\n"
                    "Please include the following information in your bug "
                    "report:\r\n"
                    "GetLastError() = 0x%08x\r\n"
                    "lifeCycle = \"%S\"\r\n"
                    "Additional Error: %S\r\n"
                    "Version: " VERSION "\r\n"
                    "Launcher Version: %S\r\n"
                    "Windows Version: %d.%d\r\n"
                    "Time Since Inject: %lu\r\n"
                    "\r\n***STACK TRACE***\r\n"
                    "%S",
                    GetLastError(), _lifeCycle, extraError,
                    _wgetenv(L"SVKEY_VERSION"),
                    osInfo.dwMajorVersion, osInfo.dwMinorVersion,
                    failTime - injectTime,
                    getStackTrace());

  MessageBoxW(NULL, buffer, L"YumePatch Helper", MB_OK | MB_ICONERROR |
                                                 MB_SETFOREGROUND);

  TerminateProcess(GetCurrentProcess(), 253);
  abort();
}

static bool memCpyHooked = false;
static int memCpyHookVer = -1;
static const uint64* memCpyStk = NULL;

static api_MemCpy orig_MemCpy;
static api_MemCpy func_MemCpy;
static LPVOID hook_MemCpy(LPVOID dst, LPCVOID src, SIZE_T len) {
  uint8* dst_ = dst;
  const uint8* src_ = src;

  if (len != KEY_SIZE) goto call;

  const uint32* u = src;
  if (u[0] != SIG(0) || u[1] != SIG(1) || u[2] != SIG(2) || u[3] != SIG(3)) {
    goto call;
  }

#ifdef DEBUG
  dwprintf(L"SVPatch: memcpy() called with key");
  getStackTrace();
#endif

  if (memCpyStk) {
    LPVOID base = GetModuleHandleW(NULL);
    LPVOID stack[16] = {0};
    uint16 frames = RtlCaptureStackBackTrace(0, 16, stack, NULL);
    for (int i = 0; i < frames; i++) { stack[i] -= (base - (LPVOID)0x1000); }
    for (int i = 1; i < 1 + *memCpyStk * 2; i += 2) {
      if (memCpyStk[i] < frames &&
          stack[memCpyStk[i]] == (LPVOID)memCpyStk[i + 1]) {
        src = getPubKey();
        goto call;
      }
    }
  } else {
    src = getPubKey();
  }

call:
  return memcpy(dst, src, len);
}

bool initMemCpyHook() {
  if (memCpyHooked) return true;
  memCpyHooked = true;

  if (func_MemCpy) {
    return MH_EnableHook(func_MemCpy) == MH_OK;
  }

  HANDLE proc = GetCurrentProcess();
  LPVOID base = (LPVOID)GetModuleHandleW(NULL);
  for (int i = 0; i < sizeof(MEMCPY_SIG) / sizeof(uint64); i += 3) {
    uint32 u[2];
    size_t n;

    dwprintf(L"SVPatch: check memcpy() @ %p (%08x %08x)",
             base + MEMCPY_SIG[i], MEMCPY_SIG[i + 1], MEMCPY_SIG[i + 2]);

    if (!ReadProcessMemory(proc, base + MEMCPY_SIG[i], u, sizeof(u), &n) ||
        n != sizeof(u)) {
      continue;
    }

    if (u[0] != MEMCPY_SIG[i + 1] || (u[1] & 0xFFFF) != (MEMCPY_SIG[i + 2] & 0xFFFF)) {
      continue;
    }

    memCpyHookVer = i / 3;

    const uint64* stk = MEMCPY_STK;
    int j = 0;
    for (int j = 0; j <= i; j++) {
      if (memCpyHookVer == j) {
        memCpyStk = *stk ? stk : NULL;
        break;
      }
      stk += (*stk * 2) + 1;
    }

    LPVOID f = base + MEMCPY_SIG[i];
    func_MemCpy = f;

    if (MH_CreateHook(f, hook_MemCpy, (LPVOID*)&orig_MemCpy) != MH_OK ||
        MH_EnableHook(f) != MH_OK) {
      break;
    }

    dwprintf(L"SVPatch: set hook on memcpy() @ %p", f);

    return true;
  }

  return false;
}

static api_SHGetSpecialFolderPathW orig_SHGetSpecialFolderPathW;
static WINAPI BOOL hook_SHGetSpecialFolderPathW(
        HWND hwnd, LPWSTR pszPath, int csidl, BOOL fCreate) {
  if (!initMemCpyHook()) {
    LIFECYCLE("INIT_MEMCPY_HOOK");
    die();
  }

  if ((!detourCommon && csidl == CSIDL_PERSONAL) ||
      ( detourCommon && (csidl == CSIDL_COMMON_APPDATA || csidl == CSIDL_LOCAL_APPDATA))) {
    wcscpy(pszPath, detourDir);
    return TRUE;
  }

  return orig_SHGetSpecialFolderPathW(hwnd, pszPath, csidl, fCreate);
}

static api_GetVolumeInformationW orig_GetVolumeInformationW;
static WINAPI BOOL hook_GetVolumeInformationW(LPCWSTR lpRootPathName,
       LPWSTR lpVolumeNameBuffer, DWORD nVolumeNameSize,
       LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
       LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer,
       DWORD nFileSystemNameSize) {
  if (!initMemCpyHook()) {
    LIFECYCLE("INIT_MEMCPY_HOOK");
    die();
  }

  return orig_GetVolumeInformationW(lpRootPathName,
         lpVolumeNameBuffer, nVolumeNameSize,
         lpVolumeSerialNumber, lpMaximumComponentLength,
         lpFileSystemFlags, lpFileSystemNameBuffer,
         nFileSystemNameSize);
}

/* Prevent network communication */
static LPVOID orig_InternetOpenW;
static WINAPI HINTERNET hook_InternetOpenW() {
  SetLastError(ERROR_NETWORK_BUSY);
  return NULL;
}

static bool initHooks() {
#define MUST(x) if ((x) != MH_OK) return false;
  MUST(MH_Initialize());

  MUST(MH_CreateHookApi(L"kernel32", "GetVolumeInformationW",
                        &hook_GetVolumeInformationW,
                        (LPVOID*)&orig_GetVolumeInformationW));
  MUST(MH_CreateHookApi(L"shell32",  "SHGetSpecialFolderPathW",
                        &hook_SHGetSpecialFolderPathW,
                        (LPVOID*)&orig_SHGetSpecialFolderPathW));
  MUST(MH_CreateHookApi(L"wininet",  "InternetOpenW",
                        &hook_InternetOpenW, &orig_InternetOpenW));

  MUST(MH_EnableHook(MH_ALL_HOOKS));
#undef MUST
  return true;
}

__declspec(dllexport) BOOL WINAPI DllMain(
                      HINSTANCE thisDll, DWORD reason, LPVOID reserved) {
  (void)reserved;

  switch (reason) {
  case DLL_PROCESS_ATTACH:
    dwprintf(L"YumePatch: initializing...");
    inDllMain = true;
    injectTime = GetTickCount();

    LIFECYCLE("GET_DETOUR_DIR");
    detourDir = _wgetenv(L"SVPATCH_DETOUR_DIR");
    if (!detourDir) die();

    detourCommon = getenv("SVPATCH_DETOUR_COMMON") &&
                   !strcmp(getenv("SVPATCH_DETOUR_COMMON"), "1");

    LIFECYCLE("PARSE_NEW_KEY");
    if (getPubKey() == NULL) die();

    LIFECYCLE("INIT_HOOKS");
    if (!initHooks()) die();

    LIFECYCLE("UNKNOWN");
    dwprintf(L"YumePatch: ready");
    inDllMain = false;

    break;
  case DLL_PROCESS_DETACH:
    MH_Uninitialize();

  }

  return TRUE;
}

#else
#if defined(__APPLE__)
#define PATH_FIND_PREFIX "/Library/Application Support/Dreamtonics/Synthesizer V Studio"
#define PATH_FIND_PREFIX_LEN (sizeof(PATH_FIND_PREFIX) - 1)

#define DYLD_INTERPOSE(_replacement,_replacee) \
  __attribute__((used)) static struct { \
    const void* replacement; \
    const void* replacee; \
  } _interpose_##_replacee __attribute__ ((section ("__DATA,__interpose"))) = { \
    (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee \
  };
#endif

static const char* _lifeCycle = "UNKNOWN";
#define LIFECYCLE(x) (_lifeCycle = x)

static void _Noreturn die() {
  int err = errno;

  fprintf(stderr, "YumePatch Helper failed to initialize properly.\n");
  fprintf(stderr, "\nPlease include the following information in your bug "
                  "report:\n"
                  "errno = %d (%s)\n"
                  "uname -srvmo: ", err, strerror(err));
  system("uname -srvmo");
  fprintf(stderr, "lifeCycle = \"%s\"\n", _lifeCycle);
  fprintf(stderr, "Version: " VERSION "\n"
                  "Launcher Version: %s\n", getenv("SVKEY_VERSION"));

  _exit(253);
  abort();
}

#if !defined(__APPLE__)
#define hook_memcpy memcpy

/* Prevent network communication */
void* curl_easy_init() { return NULL; }
int curl_easy_setopt() { return 0;    }
#endif

void* hook_memcpy(void* dst, const void* src, size_t len) {
  const uint32* u = src;

  if (len == KEY_SIZE &&
      u[0] == SIG(0) && u[1] == SIG(1) && u[2] == SIG(2) && u[3] == SIG(3)) {
    LIFECYCLE("PARSE_PUBLIC_KEY");
    src = getPubKey();
    if (!src) die();
    LIFECYCLE("UNKNOWN");
  }

  uint8* dst_ = dst;
  const uint8 *src_ = src;
  for (size_t i = 0; i < len; i++) {
    dst_[i] = src_[i];
  }

  return dst;
}

#if defined(__APPLE__)
/* Implement detouring on macOS. This is cursed. */
int hook_access(const char* path, int amode) {
  char* path_rw = (char*)((uint64)path);
  if (!getenv("SVPATCH_DETOUR_DIR") && *(getenv("SVPATCH_DETOUR_DIR")) &&
      !strcmp(path, PATH_FIND_PREFIX)) {
    LIFECYCLE("DETOUR");
    const char* tmpdir_base = getenv("TMPDIR");
    if (!tmpdir_base) die();
    if (strlen(tmpdir_base) > PATH_FIND_PREFIX_LEN) die();

    memset(path_rw, 'd', PATH_FIND_PREFIX_LEN);
    memcpy(path_rw, tmpdir_base, strlen(tmpdir_base));
    if (symlink(getenv("SVPATCH_DETOUR_DIR"), path_rw) == -1 &&
        errno != EEXIST) {
      die();
    }
    LIFECYCLE("UNKNOWN");
  }

  return access(path, amode);
}

#if !defined(__arm64__)
DYLD_INTERPOSE(hook_access, access);
DYLD_INTERPOSE(hook_memcpy, memcpy);
#else
void* real_memcpy_chk;
#endif

__attribute__((constructor)) static void ctor(void) {
  fprintf(stderr, "YumePatch " VERSION " (C) 2023 Xi Jinpwned Software.\n");
  if (!getPubKey()) die();

#if defined(__arm64__)
  int result = rebind_symbols((struct rebinding[2]){
    {"memcpy", hook_memcpy, (void *)&real_memcpy_chk}
  }, 1);
  if (result != 0) {
    die();
  }
#endif
}
#endif

#endif
