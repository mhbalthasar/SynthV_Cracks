/*
 * YumeKey: SynthV & VOICEPEAK Key Tool 2.0
 *
 * Copyright (C) 2023 Xi Jinpwned Software
 *
 * This software is made available to you under the terms of the GNU Affero
 * General Public License version 3.0. For more information, see the included
 * LICENSE.txt file.
 */

#include <cstdio>
#include <cctype>
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
using namespace juce;

#if defined(_WIN32) || defined(WIN32)
#include <cwchar>
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "tool.h"

#ifdef EXTRA_CODE
EXTRA_CODE
#endif

class StdOutLogger : public Logger {
  virtual void logMessage(const String& message) {
    std::cout << message.toStdString() << std::endl;
  }
};

class UI {
public:
  class Progress {
  protected:
    int64 value = 0;
    int64 total = 100;
  public:
    virtual ~Progress() {}

    virtual bool start() { return true; }
    virtual bool stop() { return false; }
    virtual int64 getValue() { return value; }
    virtual int64 getTotal() { return total; }

    virtual void setTitle(const String& title) = 0;
    virtual void setInfo(const String& info) = 0;
    virtual void setProgress(int64 value_, int64 total_ = -1) = 0;

    virtual bool isCancelled() { return false; }

    virtual void setTotal(int64 total_) {
      setProgress(getValue(), total_);
    }

    virtual void incr(int64 delta = 1) {
      setProgress(getValue() + delta);
    }

    virtual void decr(int64 delta = 1) {
      setProgress(getValue() - delta);
    }
  };

private:
  static bool enableGui;

#if defined(_WIN32) || defined(WIN32)
#define _(x) (x)
  class Win32Progress : public Progress {
  private:
    IProgressDialog* dialog;
    String title;
    String info;

    void refresh() {
      _(dialog)->SetTitle(title.toWideCharPointer());
      _(dialog)->SetLine(1, info.toWideCharPointer(), FALSE, NULL);
    }

  public:
    Win32Progress() {
      CoInitialize(NULL);

      HRESULT hr = CoCreateInstance(CLSID_ProgressDialog, NULL,
                                    CLSCTX_INPROC_SERVER, IID_IProgressDialog,
                                    (LPVOID *)&dialog);
      if (hr != S_OK) {
        Logger::writeToLog("Progress dialog initialization failed.");
      }

      title = "Task";
      info = "Working...";
      total = 100;
    }

    ~Win32Progress() {
      if (dialog != NULL) {
        stop();
        _(dialog)->Release();
      }
    }

    virtual bool start() {
      HRESULT hr = _(dialog)->StartProgressDialog(NULL, NULL,
                                                  PROGDLG_AUTOTIME, NULL);
      return hr == S_OK;
    }

    virtual bool stop() {
      return _(dialog)->StopProgressDialog() == S_OK;
    }

    virtual void setTitle(const String& title_) {
      title = title_;
      refresh();
    }

    virtual void setInfo(const String& info_) {
      info = info_;
      refresh();
    }

    virtual void setProgress(int64 value_, int64 total_ = -1) {
      if (total_ != -1) total = total_;
      value = value_;
      _(dialog)->SetProgress64(value, total);
    }

    virtual bool isCancelled() {
      return _(dialog)->HasUserCancelled();
    }
  };
#undef _
#endif

  class SimpleConsoleProgress : public Progress {
  private:
    bool printedInfo, stopped;
    String title;
    String info;

    /* Assuming 80 columns always; not going to bother with termios...
       The bar is 60 characters */
    const char* bar =
    "############################################################";

    void printProgress() {
      int percent = 100 * ((double)value / (total > 0 ? (double)total : 1.0));
      int barValue = 60 * ((double)value / (total > 0 ? (double)total : 1.0));

      if (value >= total)
        barValue = 59;

      fprintf(stderr, "[%3d%% (%lld/%lld)] |%.*s%*s|\r", percent, value, total,
                                                         barValue, bar,
                                                         59 - barValue, " ");
    }

  public:
    SimpleConsoleProgress() {
      printedInfo = false;
      stopped = false;
      title = "Progress";
      info = "Working...";
      value = 0;
      total = 100;
    }

    ~SimpleConsoleProgress() {
      if (!stopped) stop();
    }

    virtual bool start() {
      Logger::writeToLog("# " + title);
      Logger::writeToLog("- " + info);
      printedInfo = true;
      stopped = false;
      return true;
    }

    virtual bool stop() {
      fprintf(stderr, "\n");
      stopped = true;
      return true;
    }

    virtual void setTitle(const String& title_) {
      title = title_;
    }

    virtual void setInfo(const String& info_) {
      info = info_;
      if (printedInfo) {
        fprintf(stderr, "\n- %s\n", info.toRawUTF8());
      }
    }

    virtual void setProgress(int64 value_, int64 total_ = -1) {
      if (total_ != -1) total = total_;
      value = value_;
      printProgress();
    }

    virtual bool isCancelled() { return false; }
  };
public:

  static bool isGuiMode() { return enableGui; }
  static void setGuiMode(bool v) { enableGui = v; }

  static Progress* progressBox(const String& title = SVKEY_N,
                               const String& info = "Working...",
                               bool forceEnableGui = false) {
#if defined(_WIN32) || defined(WIN32)
    if (enableGui || forceEnableGui) {
      Progress *p = new Win32Progress();
      p->setTitle(title);
      p->setInfo(info);
      return p;
    }
#endif

    Progress *p = new SimpleConsoleProgress();
    p->setTitle(title);
    p->setInfo(info);
    return p;
  }

  static String msgBox(const String& text, const String& title = SVKEY_N,
                       const String& buttons = "ok",
                       const String& level = "") {
#if defined(_WIN32) || defined(WIN32)
    uint32 flags = MB_SETFOREGROUND;

    flags |= buttons == "ok|cancel" ? MB_OKCANCEL       :
             buttons == "retry|cancel" ? MB_RETRYCANCEL :
             buttons == "yes|no" ? MB_YESNO             :
             0;

    flags |= level == "error" ? MB_ICONERROR       :
             level == "warning" ? MB_ICONWARNING   :
             level == "info" ? MB_ICONINFORMATION  :
             level == "question" ? MB_ICONQUESTION :
             0;

    int ret = MessageBoxW(NULL, text.toWideCharPointer(),
                          title.toWideCharPointer(), flags);

    return ret == IDOK ? "ok"         :
           ret == IDCANCEL ? "cancel" :
           ret == IDRETRY ? "retry"   :
           ret == IDYES ? "yes"       :
           ret == IDNO ? "no"         :
           "";
#else
    Logger::writeToLog(level.toUpperCase() + " from " + title);
    Logger::writeToLog(text);
    Logger::writeToLog("");

    while (1) {
      char reply[256];

      printf("%s", (buttons.replace("|", "/") + "? ").toRawUTF8());
      fflush(stdout);

      if (!fgets(reply, sizeof(reply), stdin)) return "";
      Logger::writeToLog("");

      if (buttons == "ok") {
        return "ok"; /* Special case */
      }

      String s = String(CharPointer_UTF8(reply)).trim().toLowerCase();
      if (buttons == s ||
          buttons.contains("|" + s) || buttons.contains(s + "|")) {
        return s;
      }

      Logger::writeToLog("Please type the option out in full.");
    }
#endif
  }
};

class Helpers {
public:
  static uint32 svHash(const String& str, uint32 initial = 0x811c9dc5) {
    uint32 hash = initial;
    auto charPtr = str.toUTF32();

    for (int i = 0; i < str.length(); i++) {
      hash ^= (uint32)charPtr[i];
      hash *= 0x1000193;
    }
    return hash;
  }

  static void strRev(unsigned char *str, int len = -1) {
    int i, j;
    unsigned char a;

    if (len == -1) len = strlen((const char *)str);
    for (i = 0, j = len - 1; i < j; i++, j--) {
      a = str[i];
      str[i] = str[j];
      str[j] = a;
    }
  }

  static char *strDup(const char* s) {
    int l = strlen(s);
    char *ret = (char*)malloc((size_t)l);
    memcpy(ret, s, (size_t)l + 1);
    return ret;
  }

  static BigInteger b36toi(String codeStr) {
    BigInteger value = 0, step = 1;

    for (int i = 0; i < codeStr.length(); i++) {
      char c = codeStr[i];
      int v = (c >= 'A' ? c - 'A' + 10 : c - '0');
      value += (step * v);
      step *= 36;
    }

    return value;
  }

  static String itob36(BigInteger bi) {
    String ret = "";

    while (bi != 0) {
      int64 v = (bi % 36).toInt64();
      char c = v < 10 ? v + '0' : v - 10 + 'A';
      ret += c;

      bi /= 36;
    }

    return ret;
  }

  /* Deprecated: use UI::msgBox(...) instead */
  static String msgBox(const String& text, const String& title = SVKEY_N,
                       const String& buttons = "ok",
                       const String& level = "") {
    return UI::msgBox(text, title, buttons, level);
  }

  static File getDefaultSvsExe() {
    File thisApp = File::getSpecialLocation(
                   File::SpecialLocationType::currentExecutableFile);
#if defined(_WIN32) || defined(WIN32)
    if (thisApp.getSiblingFile("synthv-studio.exe").existsAsFile())
      return thisApp.getSiblingFile("synthv-studio.exe");
    File programsDir = File::getSpecialLocation(
                       File::SpecialLocationType::globalApplicationsDirectory);
    return programsDir.getChildFile("Synthesizer V Studio Pro")
                      .getChildFile("synthv-studio.exe");
#elif defined(__APPLE__)
    File programsDir = File::getSpecialLocation(
                       File::SpecialLocationType::globalApplicationsDirectory);
    return programsDir.getChildFile("Synthesizer V Studio Pro.app")
                      .getChildFile("Contents")
                      .getChildFile("MacOS")
                      .getChildFile("synthv-studio");
#else /* try to guess */
    if (thisApp.getSiblingFile("synthv-studio").existsAsFile())
      return thisApp.getSiblingFile("synthv-studio");
    File commonDir = File::getSpecialLocation(
                     File::SpecialLocationType::commonApplicationDataDirectory);
    return commonDir.getChildFile("SynthVStudioPro")
                    .getChildFile("synthv-studio");
#endif
  }

  static File getDefaultSvsDetour() {
    File documentsDir = File::getSpecialLocation(
                        File::SpecialLocationType::userDocumentsDirectory);
    return documentsDir.getChildFile("SVPatchDetour");
  }

  static File getDefaultSvsHome() {
#if defined(_WIN32) || defined(WIN32)
    File detourDir(getDefaultSvsDetour());
    return detourDir.getChildFile("Dreamtonics")
                    .getChildFile("Synthesizer V Studio");
#elif defined(__APPLE__)
    File detourDir(getDefaultSvsDetour());
    return detourDir.getChildFile("Application Support")
                    .getChildFile("Dreamtonics")
                    .getChildFile("Synthesizer V Studio");
#else
    return getDefaultSvsDetour();
#endif
  }

  static File getDefaultUnpatchedSvsHome() {
#if defined(_WIN32) || defined(WIN32)
    File documentsDir = File::getSpecialLocation(
                        File::SpecialLocationType::userDocumentsDirectory);
    return documentsDir.getChildFile("Dreamtonics")
                       .getChildFile("Synthesizer V Studio");
#else
    return File(); /* This should only be called on Windows */
#endif
  }

  static File getDefaultSvsHelper() {
    return File::getSpecialLocation(
           File::SpecialLocationType::currentExecutableFile)
#if defined(_WIN32) || defined(WIN32)
           .getSiblingFile("libsvpatch.dll");
#elif defined(__APPLE__)
           .getSiblingFile("libsvpatch.dylib");
#else
           .getSiblingFile("libsvpatch.so");
#endif
  }

  static File getDefaultVpExe() {
#if defined(_WIN32) || defined(WIN32)
    File programsDir = File::getSpecialLocation(
                       File::SpecialLocationType::globalApplicationsDirectory);
    return programsDir.getChildFile("Voicepeak")
                      .getChildFile("voicepeak.exe");
#elif defined(__APPLE__)
    File programsDir = File::getSpecialLocation(
                       File::SpecialLocationType::globalApplicationsDirectory);
    return programsDir.getChildFile("Voicepeak.app")
                      .getChildFile("Contents")
                      .getChildFile("MacOS")
                      .getChildFile("voicepeak");
#else /* try to guess */
    File commonDir = File::getSpecialLocation(
                     File::SpecialLocationType::commonApplicationDataDirectory);
    return commonDir.getChildFile("Voicepeak")
                    .getChildFile("voicepeak");
#endif
  }

  static File getDefaultVpDetour() { return getDefaultSvsDetour(); }

  static File getDefaultVpHome() {
#if defined(_WIN32) || defined(WIN32)
    File detourDir(getDefaultVpDetour());
    return detourDir.getChildFile("Dreamtonics")
                    .getChildFile("Voicepeak");
#elif defined(__APPLE__)
    File detourDir(getDefaultVpDetour());
    return detourDir.getChildFile("Application Support")
                    .getChildFile("Dreamtonics")
                    .getChildFile("Voicepeak");
#else
    return getDefaultVpDetour().getChildFile("Voicepeak");
#endif
  }

  static File getDefaultUnpatchedVpHome() {
#if defined(_WIN32) || defined(WIN32)
    File documentsDir = File::getSpecialLocation(
                        File::SpecialLocationType::commonApplicationDataDirectory);
    return documentsDir.getChildFile("Dreamtonics")
                       .getChildFile("Voicepeak");
#else
    return File(); /* This should only be called on Windows */
#endif
  }

  static File getDefaultVpHelper() { return getDefaultSvsHelper(); }
};

bool UI::enableGui = false;

class YumePatch {
private:
  File svsExeFile;
  File svsDetourDir;
  File svsHelperFile;

  RSAKey pubKey;

  bool detourCommonAppDir;
public:
  YumePatch(const File& svsExeFile_, const File& svsDetourDir_,
          const File& svsHelperFile_, const RSAKey& pubKey_,
          bool detourCommonAppDir_ = false) {
    svsExeFile = svsExeFile_;
    svsDetourDir = svsDetourDir_;
    svsHelperFile = svsHelperFile_;
    pubKey = pubKey_;
    detourCommonAppDir = detourCommonAppDir_;
  }

  bool launch() {
    return launch(StringArray());
  }

  bool launch(const String& firstArg) {
    return launch(StringArray(firstArg));
  }

  bool launch(StringArray args) {
#if defined(_WIN32) || defined(WIN32)
    LPCWSTR appNameWStr = svsExeFile.getFullPathName().toWideCharPointer();

    String cmdLine = svsExeFile.getFileName();
    for (int i = 0; i < args.size(); i++) {
      // TODO: real escaping
      cmdLine += " \"" + args[i] + "\"";
    }

    WCHAR cmdLineBuf[4096] = {};
    cmdLine.copyToUTF16(cmdLineBuf, sizeof(cmdLineBuf));

    MemoryBlock envBlock;
    LPCWSTR curEnvBlock = GetEnvironmentStringsW();

    size_t i = 0;
    while (curEnvBlock[i] != 0 || curEnvBlock[i + 1] != 0) i++;
    envBlock.append(curEnvBlock, (i + 1) * 2);

    if (getenv("SVPATCH_LEGACY_SEARCH")) {
      Logger::writeToLog("SVPatch::launch(): warning: SVPATCH_LEGACY_SEARCH "
                         "is no longer supported.");
    }

#define ENV(k, v) { \
                    String s = String(k) + String(v);      \
                    envBlock.append(s.toWideCharPointer(), \
                                   (s.length() + 1) * 2);  \
                  }
    ENV("SVPATCH_PUBKEY=", pubKey.toString());
    ENV("SVPATCH_DETOUR_DIR=", svsDetourDir.getFullPathName());
    ENV("SVKEY_VERSION=", VERSION);
    ENV("SVKEY_EXE=", File::getSpecialLocation(
                      File::SpecialLocationType::currentExecutableFile)
                      .getFullPathName());
    ENV("SVPATCH_DETOUR_COMMON=", detourCommonAppDir ? "1" : "0");
    ENV("", "");
#undef ENV

    LPVOID envBlockPtr = envBlock.getData();

    STARTUPINFOW si = {0};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {0};

    LPVOID pathPage;
    LPCWSTR helperPathWStr;
    HANDLE helperTh;
    DWORD exitCode;
    char tmp[32];

    if (!CreateProcessW(appNameWStr, cmdLineBuf, NULL, NULL, FALSE,
        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, envBlockPtr, NULL,
        &si, &pi)) {
      Logger::writeToLog("SVPatch::launch(): CreateProcessW failed.");
      return false;
    }

    pathPage = VirtualAllocEx(pi.hProcess, NULL, 0x10000,
                              MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pathPage) {
      goto failed;
    }

    helperPathWStr = svsHelperFile.getFullPathName().toUTF16();
    if (!WriteProcessMemory(pi.hProcess, pathPage, helperPathWStr,
                            wcslen(helperPathWStr) * 2, NULL)) {
      goto failed;
    }

    helperTh = CreateRemoteThread(pi.hProcess, NULL, 0,
               (LPTHREAD_START_ROUTINE)LoadLibraryW, pathPage, 0, NULL);
    if (helperTh == NULL) {
      goto failed;
    }

    if (WaitForSingleObject(helperTh, INFINITE) == WAIT_FAILED) {
      CloseHandle(helperTh);
      goto failed;
    }

    CloseHandle(helperTh);

    if (ResumeThread(pi.hThread) == (DWORD)(-1)) {
      goto failed;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
      exitCode = 0xFFFFFFFF;
      Logger::writeToLog("SVPatch::launch(): failed to retrieve exit code");
    }

    sprintf(tmp, "%d (%08x)", exitCode, exitCode);
    Logger::writeToLog("SVPatch::launch(): exitCode: " + String(tmp));

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return (exitCode == 0);

failed:
    Logger::writeToLog("SVPatch::launch(): Win32 error: " + GetLastError());
    TerminateProcess(pi.hProcess, 254);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return false;
#else
    int pid = fork();
    if (pid == 0) {
  #if defined(__APPLE__)
      setenv("DYLD_INSERT_LIBRARIES",
             svsHelperFile.getFullPathName().toRawUTF8(), 1);
  #else
      setenv("LD_PRELOAD", svsHelperFile.getFullPathName().toRawUTF8(), 1);
  #endif
      setenv("SVPATCH_PUBKEY", pubKey.toString().toRawUTF8(), 1);
      setenv("SVPATCH_DETOUR_DIR", svsDetourDir.getFullPathName().toRawUTF8(), 1);
      setenv("SVKEY_VERSION", VERSION, 1);
      setenv("SVPATCH_DETOUR_COMMON", detourCommonAppDir ? "1" : "0", 1);

      char** argv = (char**)calloc((size_t)args.size() + 2, sizeof(char*));
      argv[0] = svsDetourDir.getFullPathName() != "" ?
                Helpers::strDup(svsDetourDir.getChildFile("synthv-studio")
                                            .getFullPathName().toRawUTF8()) :
                Helpers::strDup(svsExeFile.getFullPathName().toUTF8());
      for (int i = 0; i < args.size(); i++) {
        argv[i + 1] = Helpers::strDup(args[i].toRawUTF8());
      }

      execv(static_cast<const char*>(svsExeFile.getFullPathName().toUTF8()), argv);

      perror("SVPatch::launch()");
      Logger::writeToLog("Failed to launch SynthV Studio.");
      _exit(254);
    } else if (pid != -1) {
      int status = 0xffff;
      if (waitpid(pid, &status, 0) == -1) {
        return false;
      } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("SVPatch::launch(): exited = %d, exit = %d, sig = %d\n",
               WIFEXITED(status), WEXITSTATUS(status), WTERMSIG(status));
        return false;
      }

      return true;
    }

    return false;
#endif
  }
};

class NOFS {
public:
#define NOFS_SIG_ENTRY_NAME0 "\x7F\x7F\x7F\x7F"
#define NOFS_SIG_ENTRY_NAME1 "\x7E\x7F\x7F\x7F"
#define NOFS_LIC_ENTRY_NAME  "\x7F\x7F\x7F\x7E"
#define NOFS_LIC_ENTRY_ID    0x0619EF1E
  enum EntryType {
    Block             = 0x0,
    NamedBlock        = 0x1,
    Frame             = 0x10,
    FrameNodeTable    = 0x200,
    MasterTableHeader = 0x1000,
    MasterNodeTable   = 0x2000,
  };

  struct Entry {
    uint32 id = 0;
    int64 offset = -1;
    uint32 size = 0;
    uint16 type = 0;
  };

  struct Block {
    Entry entry;
    MemoryBlock data;

    String getDataAsString() {
      return String(CharPointer_UTF8(static_cast<char*>(data.getData())),
                    data.getSize());
    }
  };

  struct NamedBlock : public Block {
    String name;
    const char* rawName;
  };

private:
  File file;
  FileInputStream fIn;

  UI::Progress* progress;

  Array<Entry> masterTable;
  HashMap<String, int> namedEntries;

  bool ready = false;
  uint32 fmtVer;
  int64 size;

  bool isLegacy = false;

  bool loadMasterTable() {
    if (progress) {
      progress->setInfo("Reading NOFS master table...");
      progress->setTotal(341); /* Theoretical maximum number of entries */
    }

    fIn.readInt();  /* Discard size */
    if (fIn.readShort() != EntryType::MasterTableHeader)
      return false; /* We expect the master table to be first */

    while (1) {
      Entry ent = {};
      ent.id = (uint32)fIn.readInt();
      ent.offset = fIn.readInt64();

      if (ent.id == 0) break;

      masterTable.add(ent);

      if (progress) progress->incr();
    }
    return true;
  }

  bool createLegacyTable() {
    /* Sometimes the master table is incomplete (useless)
       The only thing left is to parse the whole file ourselves! */
    if (getNamedString(".type") != "mu") { /* Not AI = bad master table? */
      isLegacy = true;
      masterTable.clear();

      if (progress) {
        progress->setInfo("Scanning all NOFS entries...");
        progress->setProgress(1, size);
      }

      int64 current = 0x10; /* Skip magic + version + size */
      while (!fIn.isExhausted() && current <= (size - 0x6) &&
            (!progress || !progress->isCancelled())) {

        if (progress) progress->setProgress(current);

        if (!fIn.setPosition(current)) {
          return current < (size - 0x1000) ? false : true; /* Terrible hack */
        }
        Entry ent = {};
        ent.id = (uint32)current;
        ent.offset = current;

        ent.size = (uint32)fIn.readInt();
        ent.type = (uint16)fIn.readShort();

        if (ent.type != 0x1 && ent.type != 0x10 && ent.type != 0x100 &&
            ent.type != 0x1000 && ent.type != 0x200 && ent.type != 0x2000) {
          return false;
        }

        if (ent.size == 0) {
          return current < (size - 0x1000) ? false : true; /* What? Nothing */
        }

        int i = masterTable.size();
        masterTable.add(ent);

        /* Really don't wanna call loadEntryMeta() again. It'll be slow! */
        if (ent.type == EntryType::NamedBlock) {
          MemoryBlock b;
          uint16 nameSize = (uint16)fIn.readShort();
          if (nameSize == 0) goto next; /* No name? */
          if (fIn.readIntoMemoryBlock(b, nameSize) != nameSize)
            goto next;                  /* Couldn't read entire name? */

          String name(CharPointer_UTF8(static_cast<char*>(b.getData())),
                      b.getSize());
          namedEntries.set(name, i);
        }

next:
        current += ent.size;
      }

      if (progress && progress->isCancelled()) return false;
      return true;
    }
    return true;
  }

  bool loadEntryMeta() {
    for (int i = 0; i < masterTable.size(); i++) {
      Entry& ent = masterTable.getReference(i);

      if (!fIn.setPosition(ent.offset)) return false; /* Failed to seek */
      ent.size = (uint32)fIn.readInt();
      ent.type = (uint16)fIn.readShort();

      if (ent.type == EntryType::NamedBlock) {
        MemoryBlock b;
        uint16 nameSize = (uint16)fIn.readShort();
        if (nameSize == 0) return false; /* No name? */
        if (fIn.readIntoMemoryBlock(b, nameSize) != nameSize)
          return false;                  /* Couldn't read entire name? */

        String name(CharPointer_UTF8(static_cast<char*>(b.getData())),
                    b.getSize());
        namedEntries.set(name, i);
      }
    }
    return true;
  }

  bool readNamedBlockAt(struct NamedBlock& b, int64 offset) {
    if (!fIn.setPosition(offset)) return false; /* Failed to seek */

    fIn.readInt();   /* Discard size */
    fIn.readShort(); /* Discard type */

    b.name = readPascalString();
    if (b.name.isEmpty()) return false; /* No name? */

    uint32 dataSize = (uint32)fIn.readInt();
    if (dataSize == 0) return false;  /* No data size? */
    b.data.reset();
    if (fIn.readIntoMemoryBlock(b.data, dataSize) != dataSize)
      return false;                   /* Couldn't read all the data? */

    return true;
  }

  bool writeNamedBlockAt(struct NamedBlock& b, int64 offset) {
    offset += 0x6 + 0x2 + b.name.length() + 0x4; /* Skip size, type,
                                                    name, data size */
    FileOutputStream fOut(file);
    if (!fOut.openedOk()) return false;          /* Failed to open */
    if (!fOut.setPosition(offset))
      return false;                              /* Failed to seek */

    return fOut.write(b.data.getData(), b.data.getSize());
  }

  bool appendNamedBlock(struct NamedBlock& b, bool prependTable = false) {
#define MUST(x) if (!(x)) return false
    /* Add size + type + name & length + data & length + trailer */
    b.entry.size = (uint32)(
      (uint64)4 + 2 + 2 + (uint64)b.name.length() + 4 + b.data.getSize() + 4
    );
    if (b.entry.id == 0) b.entry.id = 0xCAFEF00D;
    b.entry.type = EntryType::NamedBlock;

    FileOutputStream fOut(file);
    MUST(fOut.openedOk());

    b.entry.offset = fOut.getPosition();

    MUST(fOut.writeInt((int32)b.entry.size));
    MUST(fOut.writeShort((int16)b.entry.type));
    MUST(fOut.writeShort((int16)b.name.length()));

    if (!b.rawName) {
      MUST(fOut.write(b.name.toUTF8(), (size_t)b.name.length()));
    } else {
      MUST(fOut.write(b.rawName, strlen(b.rawName)));
    }

    MUST(fOut.writeInt((int32)b.data.getSize()));
    MUST(fOut.write(b.data.getData(), b.data.getSize()));

    /* Trailer */
    MUST(fOut.writeInt((int32)b.entry.size));

    /* Set new file size */
    int64 newSize = fOut.getPosition();
    MUST(fOut.setPosition(0x08)); /* Skip magic + version */
    fOut.writeInt64(newSize);

    if (isLegacy) { /* Format is old/weird? Find a random table to
                       stick our offset into. */
      if (progress) {
        progress->setInfo("Scanning NOFS node tables...");
        progress->setProgress(1, masterTable.size());
      }
      for (int i = 0; i < masterTable.size(); i++) {
        Entry& e = masterTable.getReference(i);
        if (e.type != EntryType::MasterNodeTable) {
          continue; /* Not interesting */
        }

        /* Look for an empty spot */
        MUST(fIn.setPosition(e.offset + 0x6));

        while (fIn.getPosition() < (e.offset + e.size)) {
          uint32 id = (uint32)fIn.readInt();
          int64 offset = fIn.readInt64();
          if (id == 0 && offset == 0) {
            MUST(!fIn.isExhausted());
            MUST(fOut.setPosition(fIn.getPosition()));
            /* Found our slot! Let's write */
            MUST(fOut.writeInt((int32)b.entry.id));
            MUST(fOut.writeInt64(b.entry.offset));
            goto success;
          }
        }

        if (progress) progress->setProgress(i + 1);
      }

      return false;
    } else { /* Sane, normal version? OK! */
      /* Skip magic + version + size + header + used entries */
      if (!prependTable) {
        int64 used = 0xC * masterTable.size();
        MUST(fOut.setPosition(0x10 + 0x6 + used));

        MUST(fOut.writeInt((int32)b.entry.id));
        MUST(fOut.writeInt64(b.entry.offset));
      } else {
        MUST(fOut.setPosition(0x10 + 0x6));

        MUST(fOut.writeInt((int32)b.entry.id));
        MUST(fOut.writeInt64(b.entry.offset));

        for (int i = 0; i < masterTable.size(); i++) {
          MUST(fOut.writeInt((int32)masterTable[i].id));
          MUST(fOut.writeInt64(masterTable[i].offset));
        }
      }
    }

success:
    masterTable.add(b.entry);
    namedEntries.set(b.name, masterTable.size() - 1);

    return true;
#undef MUST
  }

  String readPascalString() {
    uint16 strSize = (uint16)fIn.readShort();
    if (strSize == 0) return ""; /* No string? */

    MemoryBlock b;
    if (fIn.readIntoMemoryBlock(b, strSize) != strSize)
      return "";                 /* Couldn't read entire string? */

    return String(CharPointer_UTF8(static_cast<char*>(b.getData())), strSize);
  }
public:
  NOFS(File& file_, UI::Progress *progress_ = NULL) : fIn(file_) {
    file = file_;
    progress = progress_;

    if (!fIn.openedOk()) return;               /* Failed to open file    */
    if (fIn.readInt() != 0xF580) return;       /* Invalid file magic     */
    if ((fmtVer = (uint32)fIn.readInt()) > 13)
      return;                                  /* Format version too new */
    if ((size = fIn.readInt64()) != file.getSize())
      return;                                  /* File truncated         */

    if (!loadMasterTable()) return;    /* Master table loading failed    */
    if (!loadEntryMeta()) return;      /* Failed to add entry metadata   */
    if (!createLegacyTable()) return;  /* Failed to create legacy table  */

    ready = true;
  }

  const Array<Entry>* getEntries() const noexcept { return &masterTable; }
  const HashMap<String, int>* getNamedEntries() {
    return &namedEntries;
  }

  bool openedOk()                  const noexcept { return ready; }
  const uint32 getFormatVersion()  const noexcept { return fmtVer; }
  const int64 getSize()            const noexcept { return size; }

  const int getNamedEntryIndex(const String& name) const noexcept {
    if (!namedEntries.contains(name)) return -1;
    return namedEntries[name];
  }

  const bool getNamedBlock(struct NamedBlock& b, const String& name) {
    int i = getNamedEntryIndex(name);
    if (i == -1) return false;

    b.entry = masterTable[i];
    return readNamedBlockAt(b, b.entry.offset);
  }

  String getNamedString(const String& name) {
    struct NamedBlock b;
    if (!getNamedBlock(b, name)) return "";

    return b.getDataAsString();
  }

  bool fixSignature(struct NamedBlock& b,
                    const RSAKey& pubKey, const RSAKey& privKey) {
    BigInteger dataBi;
    dataBi.loadFromMemoryBlock(b.data);

    pubKey.applyToValue(dataBi);

    auto tmp = dataBi.toMemoryBlock();
    if (static_cast<unsigned char*>(tmp.getData())[253] != 0xFF) {
      return false; /* Decryption probably failed. */
    }

    privKey.applyToValue(dataBi);
    b.data = dataBi.toMemoryBlock();
    b.data.ensureSize(0x100, true);

    return writeNamedBlockAt(b, b.entry.offset);
  }

  bool getOldStyleSignature(MemoryBlock& mb) {
    if (!fIn.setPosition(size - 0x100))
      return false;

    return fIn.readIntoMemoryBlock(mb, 0x100) == 0x100;
  }

  bool setNamedBlock(struct NamedBlock& b,
                     bool noUpdate = false, bool prependTable = false) {
    auto ent = b.entry;
    if (!noUpdate && ent.offset == -1) {
      int i = getNamedEntryIndex(b.name);
      if (i != -1) b.entry = ent = masterTable[i];
    }

    if (!noUpdate && ent.offset != -1) {
      return writeNamedBlockAt(b, ent.offset);
    }

    return appendNamedBlock(b, prependTable);
  }
};

class KeyManager {
public:
  struct RSAKeyPrivateMembers { BigInteger part1, part2; };

private:
#define DREAMTONICS_PUB_KEY \
  "10001,"                  \
  "c461aca58a9ae39ab24d223b101ede8db9707d077131607fe4a18d6f8e9469d918a419" \
  "13541821fb519925868545e972920400c7d9a3879105b8f41f7c6f82f995dee6dc1aa1" \
  "6c5784935142ec3b62b2b945e6f73aa7a90c48edae153ce2cb092ad427a2114896e50a" \
  "f0e9945270f5af94836755e1efc24d55feb36eb24014acba017156a96ab08709cdf819" \
  "7a99550d5896f6cd0dc836800708be90ddbd6fa2e9b4c9ded983893f733934623976c9" \
  "01f2d0f0b30f5cbd1f1896ad6580e32db86cd2f4f20e2a31d05befac0f4ab1c0f71ef3" \
  "109921c0943c7565963da1542c0e87583e547507265c39237d5ec34b96f4dc747c385e" \
  "ba54e11741776d9a79e715"

#define DEFAULT_PUB_KEY \
  "10001,"              \
  "a84d36b60ef410d501c84b9d94c1bd1abf58601998d68916a75b2a6af0838f8d3cdc79" \
  "310bbb841f18eed18f8984bffc18a0974dd3a701543be45bef6a391403e8055115edcb" \
  "9a528200d4ebfecaff95fdda7092a593252369937ff9bed86202eb0bb10954df7b6ae7" \
  "895ea1ab974258207ca3a37cc3883b30a3c1364ee8a26ba6cafc5ed10f483265235865" \
  "21e0343c49f8414cda84e80a4dd93eca0049012130d5aeafe54b0d81003ff6a6bb3ad8" \
  "c940e31680d88a73f74c6b092e9d1136ae9329ce97207e7882ab749eee19131bc9d8a5" \
  "f22bb03c5bd7cd5ffc812aeb3a1a7f63c33b39241f59383fd31645e012ba525fd03ce1" \
  "1e52fdca99ebea86895a99"

#define DEFAULT_PRIV_KEY \
  "9b97a6b8f1621a1d92a445a3cd5ebf20f73d10bb195d5d27a058dc023990a729ffd624" \
  "047fc040092fd7b9cd656960c403509410d357561735b78c76ee510e7bfe08cc49e5a9" \
  "91662de1eef6ae7ba586594595453de5a733f1eaf7294092732a177a9b94f0ee1aff5e" \
  "46d541c98cf13cdfebdaacce2f46c6d1d4d24f0eec6b1a713624604774f62f7dec7d3b" \
  "6be96f85a9a25ae2905e7bde6396561adff98e467cada824000a5862cc18b5c778d773" \
  "c16c283b627191120d8ed60dbc36da363f4168577010874f9a7ad3a9aec12cf01fb926" \
  "5a6701c263c2f8a94129847bfb5627ba22e819674bd0f39a61df17fd7d52dff1bd6fb7" \
  "d651fe934677a82db9a2c1,"                                                \
  "a84d36b60ef410d501c84b9d94c1bd1abf58601998d68916a75b2a6af0838f8d3cdc79" \
  "310bbb841f18eed18f8984bffc18a0974dd3a701543be45bef6a391403e8055115edcb" \
  "9a528200d4ebfecaff95fdda7092a593252369937ff9bed86202eb0bb10954df7b6ae7" \
  "895ea1ab974258207ca3a37cc3883b30a3c1364ee8a26ba6cafc5ed10f483265235865" \
  "21e0343c49f8414cda84e80a4dd93eca0049012130d5aeafe54b0d81003ff6a6bb3ad8" \
  "c940e31680d88a73f74c6b092e9d1136ae9329ce97207e7882ab749eee19131bc9d8a5" \
  "f22bb03c5bd7cd5ffc812aeb3a1a7f63c33b39241f59383fd31645e012ba525fd03ce1" \
  "1e52fdca99ebea86895a99"

  static RSAKey getDreamtonicsPubKey() { return RSAKey(DREAMTONICS_PUB_KEY); }
  static RSAKey getDefaultPubKey()     { return RSAKey(DEFAULT_PUB_KEY); }
  static RSAKey getDefaultPrivKey()    { return RSAKey(DEFAULT_PRIV_KEY); }

public:
  static bool loadKey(RSAKey& keyOut, const String& path) {
    if (path.startsWith("@")) {
      if (path == "@dreamtonics.pub")
        { keyOut = getDreamtonicsPubKey(); return true; }
      if (path == "@default.pub")
        { keyOut = getDefaultPubKey(); return true; }
      if (path == "@default.key")
        { keyOut = getDefaultPrivKey(); return true; }
      return false;
    }

    File keyFile(path);
    if (!keyFile.existsAsFile() || !keyFile.hasReadAccess())
      return false;

    keyOut = RSAKey(keyFile.loadFileAsString());
    return true;
  }
};

class DeviceInfo {
private:
  int32 rawPart1;
  uint32 rawPart2;
  String fsIdHex;
  String cpuModel;

public:
  DeviceInfo() {
#if defined(WIN32) || defined(_WIN32)
    File winSysDir = File::getSpecialLocation(
                     File::SpecialLocationType::windowsSystemDirectory);
    String volRoot = winSysDir.getFullPathName().substring(0, 3);

    DWORD serNum = 0;
    if (!GetVolumeInformationW(volRoot.toWideCharPointer(),
                          NULL, 0, &serNum, NULL, NULL, NULL, 0))
      jassertfalse;

    rawPart1 = serNum;
#else
    File f("~");
    auto n = f.getFileIdentifier();
    fsIdHex = String::toHexString((int64) n);

    rawPart1 = 0;
#endif

    cpuModel = SystemStats::getCpuModel();
    rawPart2 = 0;
  }

  DeviceInfo(uint32 part1, uint32 part2) {
    rawPart1 = part1;
    rawPart2 = part2;
    fsIdHex = cpuModel = "";
  }

  DeviceInfo(const String& fsIdHex_, const String& cpuModel_) {
    rawPart1 = rawPart2 = 0;
    if (fsIdHex_[0] == '@')
      rawPart1 = fsIdHex_.substring(1).getIntValue();
    else
      fsIdHex = fsIdHex_;
    cpuModel = cpuModel_;
  }

  const int32 getRawPart1()   const noexcept { return rawPart1; }
  const uint32 getRawPart2()  const noexcept { return rawPart2; }
  const String& getFsIdHex()  const noexcept { return fsIdHex;  }
  const String& getCpuModel() const noexcept { return cpuModel; }

  String getId() const noexcept {
    String a = rawPart1 != 0 ? String(rawPart1)
                             : String(Helpers::svHash(fsIdHex));
    String b = rawPart2 != 0 ? String(rawPart2)
                             : String(Helpers::svHash(cpuModel));
    return a + "." + b;
  }
};

class ActivationCode {
private:
  BigInteger code;
  bool forSynthV;

  static bool checkCodeInt(const BigInteger& code, bool extendedCheck = false) {
    int hiBit = code.getHighestBit() + 1;
    hiBit = hiBit > 100 ? 100 : hiBit < 0 ? 0 : hiBit;

    SHA256 codeHash(code.getBitRange(0, hiBit).toMemoryBlock());
    auto codeHashMb = codeHash.getRawData();
    const uint8* codeHashData = (const uint8*)codeHashMb.getData();
    if (codeHashData[0] != 0 || (codeHashData[1] & 3) != 0)
      return false;

    if (!extendedCheck)
      return true;

    uint32 bitSum = 0;
    for (size_t i = 0; i < 96; i++) {
      bitSum += code[i];
    }

    if (code[96] != (~bitSum & 1)) {
      return false;
    }
    if (code[97] != (bitSum - (bitSum / 13 + (bitSum / 13) * 12)) < 7) {
      return false;
    }
    if ((code[98]  != bitSum % 7  < 4 ) ||
        (code[99]  != bitSum % 23 < 12) ||
         code[128] == 0) {
      return false;
    }

    uint32 a = code.getBitRangeAsInt(0,8),
           b = code.getBitRangeAsInt(4,8),
           c = code.getBitRangeAsInt(100,4);
    return ((a * b + 9) & 0xF) == c;
  }

  static void fixCodeInt(BigInteger& code) {
    uint32 a = code.getBitRangeAsInt(0,8),
           b = code.getBitRangeAsInt(4,8);
    uint32 c = (a * b + 9) & 0xF;
    code.setBitRangeAsInt(100, 4, c & 0xF);

    uint32 bitSum = 0;
    for (size_t i = 0; i < 96; i++) {
      bitSum += code[i];
    }

    code.setBit(96, ~bitSum & 1);
    code.setBit(97, (bitSum - (bitSum / 13 + (bitSum / 13) * 12)) < 7);
    code.setBit(98, bitSum % 7  < 4 );
    code.setBit(99, bitSum % 23 < 12);
    code.setBit(128, 1);
  }

public:
  ActivationCode(bool forSynthV_ = true) {
    code = 0;
    forSynthV = forSynthV_;
  }

  ActivationCode(const String& code_, bool forSynthV_ = true) {
    code = Helpers::b36toi(code_);
    forSynthV = forSynthV_;
  }

  ActivationCode(const BigInteger& code_, bool forSynthV_ = true) {
    code = code_;
    forSynthV = forSynthV_;
  }

  const bool isValid() const noexcept { return checkCodeInt(code, forSynthV); }
  String toString()    const noexcept { return Helpers::itob36(code);         }

  void generate(int64 seed = -1) {
    if (seed == -1) seed = Time::currentTimeMillis();

    Random r(seed);
    BigInteger maxCode = Helpers::b36toi("ZZZZZZZZZZZZZZZZZZZZZZZZ0");

    do {
      code = r.nextLargeNumber(maxCode);
      if (forSynthV) fixCodeInt(code);
    } while (!isValid());
  }

  static ActivationCode generateCode(int64 seed = -1, bool forSynthV = false) {
    ActivationCode ret(forSynthV);
    ret.generate(seed);
    return ret;
  }
};

class ProductInfo {
private:
  String name;
  String vendor;

public:
  ProductInfo(const String& name_, const String& vendor_) {
    name = name_;
    vendor = vendor_;
  }

  const String& getName()   const noexcept { return name; }
  const String& getVendor() const noexcept { return vendor; }

  String getId() const {
    String combined = vendor + "." + name;
    SHA256 hash (combined.toUTF8());
    return hash.toHexString();
  }

  static ProductInfo forSvs() {
    return ProductInfo("Synthesizer V Studio Pro", "Dreamtonics Co., Ltd.");
  }
};

class License {
private:
  String code = "ABCDEFGHIJKLMNOPQRSTUVWXY";
  String deviceId;
  String expiry = "2099-12-31-00-00-00";
  String productId;

public:
  License() {
    code = "";
    deviceId = "";
    expiry = "";
    productId = "";
  }

  License(const DeviceInfo& device, const ProductInfo& product) {
    deviceId = device.getId();
    productId = product.getId();
  }

  License(const String& code_, const String& deviceId_, const String& expiry_,
          const String& productId_) {
    code = code_;
    deviceId = deviceId_;
    expiry = expiry_;
    productId = productId_;
  }

  License(const MemoryBlock& b) {
    loadFromMemoryBlock(b);
  }

  License(const RSAKey& key, BigInteger& bi) {
    verify(key, bi);
  }

  const String& getCode()      const noexcept { return code; }
  const String& getDeviceId()  const noexcept { return deviceId; }
  const String& getExpiry()    const noexcept { return expiry; }
  const String& getProductId() const noexcept { return productId; }

  MemoryBlock toMemoryBlock() const {
    unsigned char buffer[256], *cursor = buffer;
    memset(buffer, 0xFF, 256);

#define addField(s) \
    (memcpy(cursor, s.toUTF8(), (size_t)s.length() + 1), \
     Helpers::strRev(cursor, s.length()),                \
     cursor += s.length() + 1)
    addField(code);
    addField(deviceId);
    addField(expiry);
    addField(productId);
#undef addField

    buffer[254] = 0x01;
    buffer[255] = 0x00;

    return MemoryBlock(buffer, 256);
  }

  bool loadFromMemoryBlock(const MemoryBlock& b) {
    if (b.getSize() != 256) jassertfalse;

    /* Almost certainly corrupted */
    if (static_cast<const unsigned char*>(b.getData())[0xF0] != 0xFF) {
      return false;
    }

    unsigned char buffer[256], *cursor = buffer;
    memcpy(buffer, b.getData(), 256);
    memset(buffer + 248, '\0', 8);

#define getField(v) \
    (Helpers::strRev(cursor),                                       \
     v = String(CharPointer_UTF8(reinterpret_cast<char*>(cursor))), \
     cursor += v.length() + 1)
    getField(code);
    getField(deviceId);
    getField(expiry);
    getField(productId);
#undef getField
    return true;
  }

  BigInteger sign(const RSAKey& key) const {
    BigInteger bi;
    bi.loadFromMemoryBlock(toMemoryBlock());
    key.applyToValue(bi);
    return bi;
  }

  bool verify(const RSAKey& key, BigInteger& bi) {
    key.applyToValue(bi);
    return loadFromMemoryBlock(bi.toMemoryBlock());
  }
};

class Commands {
private:
  static bool enableProgress;

public:
  static void parseExtraArgs(ArgumentList& args) {
    enableProgress = args.removeOptionIfFound("--progress|-#");
    UI::setGuiMode(args.removeOptionIfFound("--gui"));
  }

  static void genRsaKey(const ArgumentList& args) {
    RSAKey key, privKey;
    auto keyParts = reinterpret_cast<KeyManager::RSAKeyPrivateMembers*>(&key);

    do {
      RSAKey::createKeyPair(key, privKey, 2048, NULL, 0);
    } while (keyParts->part1.toInteger() != 0x10001);

    Logger::writeToLog("- Public Key -");
    Logger::writeToLog(key.toString());
    Logger::writeToLog("");
    Logger::writeToLog("- Private Key -");
    Logger::writeToLog(privKey.toString());
    Logger::writeToLog("");
  }

  static void genCode(const ArgumentList& args) {
    int64 seed = args.containsOption("--seed|-s")                       ?
                 args.getValueForOption("--seed|-s").getLargeIntValue() :
                 -1;

    auto code = ActivationCode::generateCode(seed,
                                !args.containsOption("--voicepeak|-V"));
    Logger::writeToLog("Code : " + code.toString());
  }

  static void checkCode(const ArgumentList& args) {
    if (args.size() < 2) {
      Logger::writeToLog("A code must be specified.");
    } else {
      ActivationCode code(args[1].text,
                          !args.containsOption("--voicepeak|-V"));
      Logger::writeToLog(code.isValid() ? "Valid code." : "Invalid code.");
    }
  }

  static void getDeviceId(const ArgumentList& args) {
    String idStr = "????";
    if (args.containsOption("--file-id|-f")) {
      String fsIdHex = args.getValueForOption("--file-id|-f"),
             cpuModel = args.getValueForOption("--cpu-model|-c");

      DeviceInfo d(fsIdHex, cpuModel);
      idStr = d.getId();
    } else {
      DeviceInfo d;
      idStr = d.getId();
    }
    Logger::writeToLog("Device ID : " + idStr);
  }

  static void getDeviceInfo(const ArgumentList& args) {
    (void)args;

    DeviceInfo d;

#if defined(WIN32) || defined(_WIN32)
    int volSer = d.getRawPart1();
    Logger::writeToLog("Volume Serial : " + String(volSer));
#else
    Logger::writeToLog("File ID (Hex) : " + d.getFsIdHex());
#endif
    Logger::writeToLog("CPU Model     : " + d.getCpuModel());
  }

  static void getProductId(const ArgumentList& args) {
    String idStr = "?????";
    if (args.containsOption("--name|-n")) {
      String name = args.getValueForOption("--name|-n");
      String vendor = args.containsOption("--vendor|-V")    ?
                      args.getValueForOption("--vendor|-V") :
                      "Dreamtonics Co., Ltd.";
      ProductInfo p(name, vendor);
      idStr = p.getId();
    } else {
      idStr = ProductInfo::forSvs().getId();
    }
    Logger::writeToLog("Product ID : " + idStr);
  }

  static void dumpLicense(const ArgumentList& args) {
    String keyPath = args.containsOption("--pubkey-file|-k")    ?
                     args.getValueForOption("--pubkey-file|-k") :
                     "@default.pub";

    File licFile(args[1].text);
    if (!licFile.existsAsFile() || !licFile.hasReadAccess()) {
      Logger::writeToLog("No such license file: " + args[1].text);
      return;
    }

    MemoryBlock licRaw;
    if (!licFile.loadFileAsData(licRaw)) {
      Logger::writeToLog("Couldn't read license data from file.");
      return;
    } else if (licRaw.getSize() != 256) {
      Logger::writeToLog("License data is " + String(licRaw.getSize()) + " "
                         "bytes, but expected 256 bytes exactly.");
      return;
    }

    License lic;
    if (!args.containsOption("--no-crypt")) {
      RSAKey key;
      if (!KeyManager::loadKey(key, keyPath)) {
        Logger::writeToLog("No such pubkey file: " + keyPath);
        return;
      }

      BigInteger licBi;
      licBi.loadFromMemoryBlock(licRaw);
      if (!lic.verify(key, licBi)) {
        Logger::writeToLog("Can't decrypt license data. Did you use the right "
                           "public key?");
        return;
      }
    } else {
      lic.loadFromMemoryBlock(licRaw);
    }

    Logger::writeToLog("Code       : " + lic.getCode());
    Logger::writeToLog("Device ID  : " + lic.getDeviceId());
    Logger::writeToLog("Expiry     : " + lic.getExpiry());
    Logger::writeToLog("Product ID : " + lic.getProductId());
  }

  static void dumpNofs(const ArgumentList& args) {
    String keyPath = args.containsOption("--pubkey-file|-k")    ?
                     args.getValueForOption("--pubkey-file|-k") :
                     "@default.pub";
    String nofsPath = args[1].text;

    RSAKey key;
    if (!KeyManager::loadKey(key, keyPath)) {
      Logger::writeToLog("No such pubkey file: " + keyPath);
      return;
    }

    File nofsFile(nofsPath);
    if (!nofsFile.existsAsFile() || !nofsFile.hasReadAccess()) {
      Logger::writeToLog("No such NOFS file: " + nofsPath);
      return;
    }

    NOFS nofs(nofsFile);
    if (!nofs.openedOk()) {
      Logger::writeToLog("Failed to open NOFS file: " + nofsPath);
      return;
    }

    String nofsType = nofs.getNamedString(".type");
    bool ai = nofsType == "mu";
    Logger::writeToLog("Name            : " + nofs.getNamedString(".name"));
    Logger::writeToLog("Vendor          : " + nofs.getNamedString(".vendor"));
    Logger::writeToLog("Version         : " + nofs.getNamedString(".version"));
    Logger::writeToLog("Language        : " + nofs.getNamedString(".language"));
    Logger::writeToLog("Extra Languages : " + nofs.getNamedString(".multi"));
    Logger::writeToLog("Phoneme Set     : " + nofs.getNamedString(".phoneset"));
    Logger::writeToLog("Type            : " + nofsType);
    Logger::writeToLog("AI              : " + String(ai ? "Yes" : "No"));

    Logger::writeToLog("");

    if (args.containsOption("--even-more-extra")) {
      auto named = nofs.getNamedEntries();
      Logger::writeToLog("Named Entries   : " + String(named->size()));

      for (auto it = named->begin(); it.next();) {
        String name = it.getKey();
        bool isAllPrintable = true;
        const char* nameRaw = name.toRawUTF8();
        for (int i = 0; i < name.length(); i++) {
          char c = nameRaw[i];
          if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') || (c == '.' || c == '-' || c == ' '))) {
            isAllPrintable = false;
            break;
          }
        }

        if (!isAllPrintable)
          name = "HEX{" + String::toHexString(name.toRawUTF8(),
                                              name.length()).replace(" ", "") +
                                              "}";
        else
          name += " HEX{" + String::toHexString(name.toRawUTF8(),
                                                name.length())
                                                .replace(" ", "") + "}";

        struct NOFS::Entry ent = nofs.getEntries()->
                                 getUnchecked(it.getValue());

        Logger::writeToLog("- " + name + " "
                           "(table slot: " + String(it.getValue()) + ","
                           " size: " + String(ent.size) + ","
                           " offset: " + String(ent.offset) + ")");
      }

      Logger::writeToLog("");

      auto entries = nofs.getEntries();
      Logger::writeToLog("Total Entries   : " + String(entries->size()));

      for (int i = 0; i < entries->size(); i++) {
        struct NOFS::Entry ent = entries->getUnchecked(i);

        Logger::writeToLog("- id: " + String::toHexString(ent.id) + " "
                           "(table slot: " + String(i) + ","
                           " size: " + String(ent.size) + ","
                           " type: " + String::toHexString(ent.type) + "," +
                           ((ent.size > 256 && ent.size < 512) ?
                              " -*- may contain a signature -*- ," : "") +
                           " offset: " + String(ent.offset) + ")");
      }
    }

    struct NOFS::NamedBlock sigRaw;
    if (!nofs.getNamedBlock(sigRaw, NOFS_SIG_ENTRY_NAME0)) {
      Logger::writeToLog("<!> Warning: signature missing (0)");
    } else {
      // TODO: signature verification

      if (args.containsOption("--extra")) {
        Logger::writeToLog("Raw Signature (0)   : " +
                           String::toHexString(sigRaw.data.getData(),
                                          (int)sigRaw.data.getSize()));

        BigInteger sigBi;
        sigBi.loadFromMemoryBlock(sigRaw.data);
        key.applyToValue(sigBi);

        auto dec = sigBi.toMemoryBlock();
        if (static_cast<unsigned char*>(dec.getData())[253] != 0xFF) {
          Logger::writeToLog("<!> Signature invalid. Wrong pubkey file?");
        }

        Logger::writeToLog("Decrypted Signature : " +
                           String::toHexString(dec.getData(),
                                          (int)dec.getSize()));

        Logger::writeToLog("");
      }
    }

    if (!nofs.getNamedBlock(sigRaw, NOFS_SIG_ENTRY_NAME1)) {
      Logger::writeToLog("<!> Warning: signature missing (1)");
    } else {
      // TODO: signature verification

      if (args.containsOption("--extra")) {
        Logger::writeToLog("Raw Signature (1)   : " +
                           String::toHexString(sigRaw.data.getData(),
                                          (int)sigRaw.data.getSize()));

        BigInteger sigBi;
        sigBi.loadFromMemoryBlock(sigRaw.data);
        key.applyToValue(sigBi);

        auto dec = sigBi.toMemoryBlock();
        if (static_cast<unsigned char*>(dec.getData())[253] != 0xFF) {
          Logger::writeToLog("<!> Signature invalid. Wrong pubkey file?");
        }

        Logger::writeToLog("Decrypted Signature : " +
                           String::toHexString(dec.getData(),
                                          (int)dec.getSize()));

        Logger::writeToLog("");
      }
    }

    struct NOFS::NamedBlock licRaw;
    if (!nofs.getNamedBlock(licRaw, NOFS_LIC_ENTRY_NAME)) {
      Logger::writeToLog("Activated       : No");
    } else {
      BigInteger licBi;
      licBi.loadFromMemoryBlock(licRaw.data);

      License lic;
      if (!lic.verify(key, licBi)) {
        Logger::writeToLog("Activated       : (?) decryption error");
      } else {
        if (ai && licRaw.entry.id != NOFS_LIC_ENTRY_ID) {
          Logger::writeToLog("Activated       : (?) bad entry ID");
        } else {
          Logger::writeToLog("Activated       : Yes");
        }
        Logger::writeToLog("- Code          : " + lic.getCode());
        Logger::writeToLog("- Device ID     : " + lic.getDeviceId());
        Logger::writeToLog("- Expiry        : " + lic.getExpiry());
        Logger::writeToLog("- Product ID    : " + lic.getProductId());
      }
    }
  }

  static void fixNofs(const ArgumentList& args) {
    String keyPath = args.containsOption("--pubkey-file|-k")    ?
                     args.getValueForOption("--pubkey-file|-k") :
                     "@dreamtonics.pub";
    String privPath = args.containsOption("--privkey-file|-p")    ?
                      args.getValueForOption("--privkey-file|-p") :
                      "@default.key";
    String oldNofsPath = args[1].text;
    String outPath = args[2].text;

    RSAKey key;
    if (!KeyManager::loadKey(key, keyPath)) {
      Logger::writeToLog("No such pubkey file: " + keyPath);
      return;
    }

    RSAKey privKey;
    if (!KeyManager::loadKey(privKey, privPath)) {
      Logger::writeToLog("No such privkey file: " + privPath);
      return;
    }

    File nofsFile(oldNofsPath);
    if (!nofsFile.existsAsFile() || !nofsFile.hasReadAccess()) {
      Logger::writeToLog("No such NOFS file: " + oldNofsPath);
      return;
    }

    File outFile(outPath);
    if (outFile.existsAsFile() &&
        outFile.getFullPathName() == nofsFile.getFullPathName()) {
      Logger::writeToLog("In-place fixing is not supported.");
      return;
    }

    if (!nofsFile.copyFileTo(outFile)) {
      Logger::writeToLog("Failed to copy NOFS template.");
      return;
    }

    NOFS nofs(outFile);
    if (!nofs.openedOk()) {
      Logger::writeToLog("Failed to open NOFS file: " + outPath);
      return;
    }

    struct NOFS::NamedBlock sigRaw = {};
    if (!nofs.getNamedBlock(sigRaw, NOFS_SIG_ENTRY_NAME0)) {
      Logger::writeToLog("<!> Warning: signature missing (0)");
    } else {
      if (!nofs.fixSignature(sigRaw, key, privKey))
        Logger::writeToLog("Failed to fix signature (0).");
    }

    if (!nofs.getNamedBlock(sigRaw, NOFS_SIG_ENTRY_NAME1)) {
      Logger::writeToLog("<!> Warning: signature missing (1)");
    } else {
      if (!nofs.fixSignature(sigRaw, key, privKey))
        Logger::writeToLog("Failed to fix signature (1).");
    }

    Logger::writeToLog("Wrote updated signature.");
  }

  static void activateNofs(const ArgumentList& args) {
    String privPath = args.containsOption("--privkey-file|-p")    ?
                      args.getValueForOption("--privkey-file|-p") :
                      "@default.key";
    String oldNofsPath = args[1].text;
    String outPath = args[2].text;

    RSAKey privKey;
    if (!KeyManager::loadKey(privKey, privPath)) {
      Logger::writeToLog("No such privkey file: " + privPath);
      return;
    }

    File nofsFile(oldNofsPath);
    if (!nofsFile.existsAsFile() || !nofsFile.hasReadAccess()) {
      Logger::writeToLog("No such NOFS file: " + oldNofsPath);
      return;
    }

    File outFile(outPath);
    if (outFile.existsAsFile() &&
        outFile.getFullPathName() == nofsFile.getFullPathName()) {
      Logger::writeToLog("In-place activation is not supported.");
      return;
    }

    if (!nofsFile.copyFileTo(outFile)) {
      Logger::writeToLog("Failed to copy NOFS template.");
      return;
    }

    NOFS nofs(outFile);
    if (!nofs.openedOk()) {
      Logger::writeToLog("Failed to open NOFS file: " + outPath);
      return;
    }

    ProductInfo p(nofs.getNamedString(".name"),
                  nofs.getNamedString(".vendor"));
    License lic;

    if (args.containsOption("--file-id|-f")) {
      String fsIdHex = args.getValueForOption("--file-id|-f"),
             cpuModel = args.getValueForOption("--cpu-model|-c");

      DeviceInfo d(fsIdHex, cpuModel);
      lic = License(d, p);
    } else {
      DeviceInfo d;
      lic = License(d, p);
    }

    BigInteger licSigned = lic.sign(privKey);

    struct NOFS::NamedBlock b = {};
    b.entry.id = NOFS_LIC_ENTRY_ID;
    b.rawName = NOFS_LIC_ENTRY_NAME;
    b.name = NOFS_LIC_ENTRY_NAME;
    b.data = licSigned.toMemoryBlock();
    nofs.setNamedBlock(b, false, true);

    Logger::writeToLog("Activated.");
  }

  static void activateAny(const ArgumentList& args) {
    String name = args.getValueForOption("--name|-n");
    String vendor = args.containsOption("--vendor|-V")    ?
                    args.getValueForOption("--vendor|-V") :
                    "Dreamtonics Co., Ltd.";
    String privPath = args.containsOption("--privkey-file|-p")    ?
                      args.getValueForOption("--privkey-file|-p") :
                      "@default.key";
    String licPath = args[1].text;

    RSAKey privKey;
    if (!KeyManager::loadKey(privKey, privPath)) {
      Logger::writeToLog("No such privkey file: " + privPath);
      return;
    }

    File licFile(licPath);
    if (!licFile.create()) {
      Logger::writeToLog("Failed to create license file: " + licPath);
      return;
    }

    ProductInfo p(name, vendor);
    if (name == "") {
      p = ProductInfo::forSvs();
    }

    License lic;

    if (args.containsOption("--file-id|-f")) {
      String fsIdHex = args.getValueForOption("--file-id|-f"),
             cpuModel = args.getValueForOption("--cpu-model|-c");

      DeviceInfo d(fsIdHex, cpuModel);
      lic = License(d, p);
    } else {
      DeviceInfo d;
      lic = License(d, p);
    }

    BigInteger licSigned = lic.sign(privKey);
    MemoryBlock licRaw = licSigned.toMemoryBlock();
    licRaw.ensureSize(0x100, true);

    if (!licFile.replaceWithData(licRaw.getData(), licRaw.getSize())) {
      Logger::writeToLog("Failed to write license data.");
      return;
    }

    Logger::writeToLog("License saved.");
  }

  static void installSvpk(const ArgumentList& args) {
    String keyPath = args.containsOption("--pubkey-file|-k")    ?
                     args.getValueForOption("--pubkey-file|-k") :
                     "@dreamtonics.pub";
    String privPath = args.containsOption("--privkey-file|-p")    ?
                      args.getValueForOption("--privkey-file|-p") :
                      "@default.key";
    String dbPath = args.containsOption("--svs-databases-path|-d")   ?
                    args.getValueForOption("--svs-databases-path|-d") :
                    "databases";
    String svpkPath = args[1].text;

    auto p = std::unique_ptr<UI::Progress>(enableProgress ?
                   UI::progressBox("SVKey SVPK Installer", "Preparing...") :
                   NULL);

    if (p) p->start();

    RSAKey key;
    if (!KeyManager::loadKey(key, keyPath)) {
      Logger::writeToLog("No such pubkey file: " + keyPath);
      return;
    }

    RSAKey privKey;
    if (!KeyManager::loadKey(privKey, privPath)) {
      Logger::writeToLog("No such privkey file: " + privPath);
      return;
    }

    File svpkFile(svpkPath);
    if (!svpkFile.existsAsFile() || !svpkFile.hasReadAccess()) {
      Logger::writeToLog("No such SVPK file: " + svpkPath);
      return;
    }

    File dbDir(dbPath);
    if (!dbDir.isDirectory() && !dbDir.createDirectory().wasOk()) {
      Logger::writeToLog("No such databases directory: " + dbPath);
      return;
    }

    /* SVPK breaks the ZIP file format. We have to fix it. */
    MemoryBlock svpkData;
    if (!svpkFile.loadFileAsData(svpkData)) {
      Logger::writeToLog("Failed to read SVPK file.");
      return;
    }

    if (p) {
      p->setInfo("Reading SVPK...");
      p->setProgress(10, 100);
    }

    auto svpkDataPtr = static_cast<unsigned char*>(svpkData.getData());
    size_t svpkSize = svpkData.getSize();

    if (svpkDataPtr[0] != 'S' || svpkDataPtr[1] != 'V' ||
        svpkDataPtr[2] != 'P' || svpkDataPtr[3] != 'K') {
      if (args.containsOption("--force")) {
        Logger::writeToLog("<!> Warning: SVPK header is invalid");
      } else {
        Logger::writeToLog("Invalid SVPK header.");
        return;
      }
    } else {
      if (svpkData.getSize() < 0x108) {
        Logger::writeToLog("SVPK file is truncated.");
        return;
      }

      svpkDataPtr += 0x108;
      svpkSize -= 0x108;
    }

    MemoryInputStream svpkStream(svpkDataPtr, svpkSize, false);
    ZipFile svpkZip(svpkStream);
    if (svpkZip.getNumEntries() == 0) {
      Logger::writeToLog("Internal ZIP failure.");
      return;
    }

    if (args.containsOption("--extra")) {
      Logger::writeToLog("SVPK contains " + String(svpkZip.getNumEntries()) +
                         " entries.");
      for (int i = 0; i < svpkZip.getNumEntries(); i++) {
        Logger::writeToLog("- " + svpkZip.getEntry(i)->filename);
      }
    }

    if (p) {
      p->setInfo("Reading SVPK metadata...");
      p->setProgress(20, 100);
    }

    int pkgCfgIdx = svpkZip.getIndexOfFileName("package-config");
    if (pkgCfgIdx == -1) {
      Logger::writeToLog("Package is missing configuration metadata.");
      return;
    }
    auto pkgCfgStream = svpkZip.createStreamForEntry(pkgCfgIdx);
    if (!pkgCfgStream) {
      Logger::writeToLog("Failed to read package configuration metadata.");
      return;
    }
    String pkgCfgTxt = pkgCfgStream->readEntireStreamAsString();
    delete pkgCfgStream;
    if (pkgCfgTxt.isEmpty()) {
      Logger::writeToLog("Failed to read package configuration metadata stream.");
      return;
    }
    auto pkgCfgJson = JSON::parse(pkgCfgTxt);
    if (pkgCfgJson.isVoid()) {
      Logger::writeToLog("Failed to parse package configuration metadata.");
      Logger::writeToLog(pkgCfgTxt);
      return;
    }

    ProductInfo product(pkgCfgJson["productName"], pkgCfgJson["vendorName"]);

    if (p) {
      p->setTitle("SVKey SVPK Installer: " + product.getName());
    }

    File installDir = dbDir.getChildFile(product.getName());
    if (!installDir.isDirectory() && !installDir.createDirectory().wasOk()) {
      Logger::writeToLog("Failed to create installation directory.");
    }

    File voiceNofsFile;

    if (p) {
      p->setInfo("Unpacking...");
      p->setProgress(0, svpkZip.getNumEntries() - 1);
    }

    for (int i = 0; i < svpkZip.getNumEntries(); i++) {
      const ZipFile::ZipEntry* e = svpkZip.getEntry(i);
      if (e->filename == "package-config") continue;

      if (e->filename == "voice.nofs") { /* Special case */
        voiceNofsFile = installDir.getChildFile(String("voice.$.nofs")
                        .replace("$", pkgCfgJson["versionNumber"].toString()));
        if (!voiceNofsFile.create()) {
          Logger::writeToLog("Failed to create voice.nofs file.");
          return;
        }

        FileOutputStream voiceNofsOut(voiceNofsFile);
        if (!voiceNofsOut.openedOk() || !voiceNofsOut.setPosition(0) ||
            !voiceNofsOut.truncate()) {
          Logger::writeToLog("Failed to prepare writing voice.nofs file.");
          return;
        }

        auto voiceNofsStream = svpkZip.createStreamForEntry(i);
        if (!voiceNofsStream) {
          Logger::writeToLog("Failed to read voice.nofs data.");
          return;
        }

        int64 total = voiceNofsOut.writeFromInputStream(*voiceNofsStream, -1);
        if (total != e->uncompressedSize) {
          Logger::writeToLog("Failed to write voice.nofs data.");
          return;
        }

        delete voiceNofsStream;
        continue;
      }

      if (!svpkZip.uncompressEntry(i, installDir, true).wasOk()) {
        Logger::writeToLog("Failed to unpack file: " + e->filename);
        return;
      }

      if (p) p->incr();
    }

    Logger::writeToLog("Installed : " + installDir.getFullPathName());
    if (args.containsOption("--no-fix")) return;

    NOFS nofs(voiceNofsFile, p.get());
    if (!nofs.openedOk()) {
      Logger::writeToLog("<!> Warning: failed to patch voice.nofs");
      return;
    }

    if (p) {
      p->setInfo("Patching signatures...");
      p->setProgress(70, 100);
    }

    struct NOFS::NamedBlock sigRaw;
    if (!nofs.getNamedBlock(sigRaw, NOFS_SIG_ENTRY_NAME0)) {
      Logger::writeToLog("<!> Warning: signature missing (0)");
    } else {
      if (!nofs.fixSignature(sigRaw, key, privKey))
        Logger::writeToLog("Failed to fix signature (0).");
    }

    if (p) p->setProgress(90, 100);

    if (!nofs.getNamedBlock(sigRaw, NOFS_SIG_ENTRY_NAME1)) {
      Logger::writeToLog("<!> Warning: signature missing (1)");
    } else {
      if (!nofs.fixSignature(sigRaw, key, privKey))
        Logger::writeToLog("Failed to fix signature (1).");
    }

    Logger::writeToLog("Wrote updated signature.");

    if (args.containsOption("--no-activate")) return;

    if (p) {
      p->setInfo("Activating...");
      p->setProgress(95, 100);
    }

    License lic;
    if (args.containsOption("--file-id|-f")) {
      String fsIdHex = args.getValueForOption("--file-id|-f"),
             cpuModel = args.getValueForOption("--cpu-model|-c");

      DeviceInfo d(fsIdHex, cpuModel);
      lic = License(d, product);
    } else {
      DeviceInfo d;
      lic = License(d, product);
    }

    BigInteger licSigned = lic.sign(privKey);

    struct NOFS::NamedBlock b = {};
    b.entry.id = NOFS_LIC_ENTRY_ID;
    b.rawName = NOFS_LIC_ENTRY_NAME;
    b.name = NOFS_LIC_ENTRY_NAME;
    b.data = licSigned.toMemoryBlock();
    nofs.setNamedBlock(b, false, true);

    Logger::writeToLog("Activated.");
  }

  static void installVppk(const ArgumentList& args) {
    String keyPath = args.containsOption("--pubkey-file|-k")    ?
                     args.getValueForOption("--pubkey-file|-k") :
                     "@dreamtonics.pub";
    String privPath = args.containsOption("--privkey-file|-p")    ?
                      args.getValueForOption("--privkey-file|-p") :
                      "@default.key";
    String dbPath = args.containsOption("--vp-storage-path|-d")    ?
                    args.getValueForOption("--vp-storage-path|-d") :
                    "storage";
    String keyDbPath = args.containsOption("--vp-keys-path|-K")    ?
                       args.getValueForOption("--vp-keys-path|-K") :
                       "keys";
    String vppkPath = args[1].text;

    auto p = std::unique_ptr<UI::Progress>(enableProgress ?
                   UI::progressBox("SVKey VPPK Installer", "Preparing...") :
                   NULL);

    if (p) p->start();

    RSAKey key;
    if (!KeyManager::loadKey(key, keyPath)) {
      Logger::writeToLog("No such pubkey file: " + keyPath);
      return;
    }

    RSAKey privKey;
    if (!KeyManager::loadKey(privKey, privPath)) {
      Logger::writeToLog("No such privkey file: " + privPath);
      return;
    }

    File vppkFile(vppkPath);
    if (!vppkFile.existsAsFile() || !vppkFile.hasReadAccess()) {
      Logger::writeToLog("No such VPPK file: " + vppkPath);
      return;
    }

    File dbDir(dbPath);
    if (!dbDir.isDirectory() && !dbDir.createDirectory().wasOk()) {
      Logger::writeToLog("No such databases directory: " + dbPath);
      return;
    }

    ZipFile vppkZip(vppkFile);
    if (vppkZip.getNumEntries() == 0) {
      Logger::writeToLog("Internal ZIP failure.");
      return;
    }

    if (args.containsOption("--extra")) {
      Logger::writeToLog("VPPK contains " + String(vppkZip.getNumEntries()) +
                         " entries.");
      for (int i = 0; i < vppkZip.getNumEntries(); i++) {
        Logger::writeToLog("- " + vppkZip.getEntry(i)->filename);
      }
    }

    if (p) {
      p->setInfo("Reading VPPK metadata...");
      p->setProgress(20, 100);
    }

    int pkgCfgIdx = vppkZip.getIndexOfFileName("package-config");
    if (pkgCfgIdx == -1) {
      Logger::writeToLog("Package is missing configuration metadata.");
      return;
    }
    auto pkgCfgStream = vppkZip.createStreamForEntry(pkgCfgIdx);
    if (!pkgCfgStream) {
      Logger::writeToLog("Failed to read package configuration metadata.");
      return;
    }
    String pkgCfgTxt = pkgCfgStream->readEntireStreamAsString();
    delete pkgCfgStream;
    if (pkgCfgTxt.isEmpty()) {
      Logger::writeToLog("Failed to read package configuration metadata stream.");
      return;
    }
    auto pkgCfgJson = JSON::parse(pkgCfgTxt);
    if (pkgCfgJson.isVoid()) {
      Logger::writeToLog("Failed to parse package configuration metadata.");
      Logger::writeToLog(pkgCfgTxt);
      return;
    }

    ProductInfo product(pkgCfgJson["productName"], pkgCfgJson["vendorName"]);

    if (p) {
      p->setTitle("YumeKey VPPK Installer: " + product.getName());
    }

    File installDir = dbDir;
    if (!installDir.isDirectory() && !installDir.createDirectory().wasOk()) {
      Logger::writeToLog("Failed to create installation directory.");
    }

    File voiceNofsFile;

    if (p) {
      p->setInfo("Unpacking...");
      p->setProgress(0, vppkZip.getNumEntries() - 1);
    }

    for (int i = 0; i < vppkZip.getNumEntries(); i++) {
      const ZipFile::ZipEntry* e = vppkZip.getEntry(i);
      if (e->filename == "package-config") continue;

      if (!vppkZip.uncompressEntry(i, installDir, true).wasOk()) {
        Logger::writeToLog("Failed to unpack file: " + e->filename);
        return;
      }

      if (p) p->incr();
    }

    Logger::writeToLog("Installed : " + installDir.getFullPathName());

    if (args.containsOption("--no-activate")) return;

    if (p) {
      p->setInfo("Activating...");
      p->setProgress(95, 100);
    }

    File licFile = File(keyDbPath).getChildFile(product.getId() + ".vpk");
    if (!licFile.create()) {
      Logger::writeToLog("Failed to create license file:");
      return;
    }

    License lic;
    if (args.containsOption("--file-id|-f")) {
      String fsIdHex = args.getValueForOption("--file-id|-f"),
             cpuModel = args.getValueForOption("--cpu-model|-c");

      DeviceInfo d(fsIdHex, cpuModel);
      lic = License(d, product);
    } else {
      DeviceInfo d;
      lic = License(d, product);
    }

    BigInteger licSigned = lic.sign(privKey);

    MemoryBlock licRaw = licSigned.toMemoryBlock();
    licRaw.ensureSize(0x100, true);

    if (!licFile.replaceWithData(licRaw.getData(), licRaw.getSize())) {
      Logger::writeToLog("Failed to write license data.");
      return;
    }

    Logger::writeToLog("Activated.");
  }

  static void easy(const ArgumentList& argsConst) {
    ArgumentList args(argsConst);

    UI::setGuiMode(true);
    enableProgress = true;
#if defined(WIN32) || defined(_WIN32)
    if (!args.removeOptionIfFound("--cli"))
      FreeConsole();
#endif

    File svsExeFile = args.containsOption("--svs-exe|-x")             ?
                      File(args.removeValueForOption("--svs-exe|-x")) :
                      Helpers::getDefaultSvsExe();
    File svsDetourDir = args.containsOption("--svs-detour|-D")             ?
                        File(args.removeValueForOption("--svs-detour|-D")) :
                        Helpers::getDefaultSvsDetour();
    File svsHomeDir = args.containsOption("--svs-home|-H")             ?
                      File(args.removeValueForOption("--svs-home|-H")) :
                      Helpers::getDefaultSvsHome();
    File svsHelperFile = args.containsOption("--helper-library|-l")          ?
                      File(args.removeValueForOption("--helper-library|-l")) :
                      Helpers::getDefaultSvsHelper();
    String keyPath = args.containsOption("--pubkey-file|-k")       ?
                     args.removeValueForOption("--pubkey-file|-k") :
                     "@default.pub";

#if !defined(WIN32) && !defined(_WIN32) && !defined(__APPLE__)
    /* Disable detouring by default on Linux */
    if (!args.removeOptionIfFound("--enable-detour")) {
      svsDetourDir = File();
      svsHomeDir = svsExeFile.getParentDirectory();
    }
#else
    args.removeOptionIfFound("--enable-detour");
#endif

    RSAKey key;
    if (!KeyManager::loadKey(key, keyPath)) {
      Logger::writeToLog("No such pubkey file: " + keyPath);
      return;
    }

    if (!svsExeFile.existsAsFile()) {
      Helpers::msgBox("Can't find SynthV Studio executable", SVKEY_N,
                      "ok", "warning");
    }

    if (!svsHelperFile.existsAsFile()) {
      Helpers::msgBox("Can't find YumePatch helper library", SVKEY_N,
                      "ok", "warning");
    }

    if (!svsDetourDir.getFullPathName().isEmpty() &&
        !svsDetourDir.isDirectory() &&
        !svsDetourDir.createDirectory().wasOk()) {
      Helpers::msgBox("Can't create YumePatch detour directory.", SVKEY_N,
                      "ok", "warning");
    }

    if (!svsHomeDir.isDirectory() && !svsHomeDir.createDirectory().wasOk()) {
      Helpers::msgBox("Can't create YumePatch home directory.", SVKEY_N,
                      "ok", "warning");
    }

#if defined(WIN32) || defined(_WIN32)
    /* Install skeleton files like scripts from base SynthV data, if needed */
    File svsHomeDirDocsThirdParty = svsHomeDir.getChildFile("docs")
                                              .getChildFile("third-party");
    File svsHomeDirScripts = svsHomeDir.getChildFile("scripts");
    File svsHomeDirDicts = svsHomeDir.getChildFile("dicts");
    File svsHomeDirTranslations = svsHomeDir.getChildFile("translations");

    if ((!svsHomeDirScripts.isDirectory() || !svsHomeDirDicts.isDirectory() ||
         !svsHomeDirTranslations.isDirectory()) &&
         Helpers::msgBox("Some files used by SynthV Studio are missing. "
                         "Install them?", SVKEY_N, "yes|no",
                         "question") == "yes") {

      File svKeyLicenseDoc = File::getSpecialLocation(
                             File::SpecialLocationType::currentExecutableFile)
                             .getSiblingFile("LICENSE.txt");
      if (svKeyLicenseDoc.existsAsFile() && svKeyLicenseDoc.hasReadAccess()) {
        /* Add our license document to the third party docs directory */
        if (svsHomeDirDocsThirdParty.isDirectory() ||
            svsHomeDirDocsThirdParty.createDirectory()) {
          svKeyLicenseDoc.copyFileTo(svsHomeDirDocsThirdParty
                                     .getChildFile("YumeKey.txt"));
        }
      }

      File unpatchedSvsHomeDir = Helpers::getDefaultUnpatchedSvsHome();
      File unpatchedSvsHomeDirScripts = unpatchedSvsHomeDir
                                        .getChildFile("scripts");

      File unpatchedSvsHomeDirDicts = unpatchedSvsHomeDir.getChildFile("dicts");
      File unpatchedSvsHomeDirTranslations = unpatchedSvsHomeDir
                                             .getChildFile("translations");

      bool v = unpatchedSvsHomeDirScripts.copyDirectoryTo(svsHomeDirScripts) &&
               unpatchedSvsHomeDirDicts.copyDirectoryTo(svsHomeDirDicts)     &&
               unpatchedSvsHomeDirTranslations
               .copyDirectoryTo(svsHomeDirTranslations);

      if (!v) {
        Helpers::msgBox("Some files could not be copied. Certain features of "
                        "SynthV Studio may not function properly.", SVKEY_N,
                        "ok", "error");
      }
    }
#endif

    YumePatch patcher(svsExeFile, svsDetourDir, svsHelperFile, key);

    File licFile = svsHomeDir.getChildFile("license")
                             .getChildFile("license.bin");
    if (!licFile.existsAsFile() || !licFile.hasReadAccess() ||
         licFile.getSize() != 256) {
      if (Helpers::msgBox("SynthV Studio is not yet activated. Activate now?",
                          SVKEY_N, "yes|no", "question") == "yes") {
        File licDir = licFile.getParentDirectory();
        if (!licDir.isDirectory() && !licDir.createDirectory().wasOk()) {
          Helpers::msgBox("Failed to create license directory.", SVKEY_N,
                          "ok", "error");
        } else {
          licFile.create();
          ArgumentList subArgs("-", StringArray("-",
                                                licFile.getFullPathName()));
          Commands::activateAny(subArgs);
        }
      }
    } else {
      DeviceInfo d;
      License lic;
      MemoryBlock licRaw;
      BigInteger licBi;
      if (!licFile.loadFileAsData(licRaw) ||
          !(licBi.loadFromMemoryBlock(licRaw), lic.verify(key, licBi)) ||
          lic.getDeviceId() != d.getId()) {
        if (Helpers::msgBox("The license data is corrupt. Repair now?\n\n"
                            "If you do not, SynthV Studio will likely not "
                            "function properly.", SVKEY_N,
                            "yes|no", "warning") == "yes") {
          ArgumentList subArgs("-", StringArray("-",
                                                licFile.getFullPathName()));
          Commands::activateAny(subArgs);
        }
      }
    }

    String firstArg = args[0].text == "--easy" ? args[1].text : args[0].text;

    if (firstArg.endsWith(".svpk")) {
      /* Install SVPK */

      File dbDir = svsHomeDir.getChildFile("databases");
      ArgumentList subArgs("-", StringArray("-",
                                firstArg, "-d", dbDir.getFullPathName()));

      Commands::installSvpk(subArgs);
      if (Helpers::msgBox("Installed " + firstArg + "\nLaunch SynthV Studio now?",
                          SVKEY_N, "yes|no", "question") != "yes")
        return;

      if (!patcher.launch()) goto failed_launch;
      return;
    }

    if (firstArg.endsWith(".svp")) {
      if (!patcher.launch(firstArg)) goto failed_launch;
      return;
    }

    if (patcher.launch())
      return;

failed_launch:
    Helpers::msgBox("SynthV Studio failed to start properly.\n\n"
                    "This could be due to a compatibility error with the "
                    "patch or an installed file. If this recently started "
                    "happening, please remove any installed voice databases "
                    "and try again.", SVKEY_N, "ok", "error");
  }

  static void vpEasy(const ArgumentList& argsConst) {
    ArgumentList args(argsConst);

    UI::setGuiMode(true);
    enableProgress = true;
#if defined(WIN32) || defined(_WIN32)
    if (!args.removeOptionIfFound("--cli"))
      FreeConsole();
#endif

    File vpExeFile = args.containsOption("--vp-exe|-x")              ?
                      File(args.removeValueForOption("--vp-exe|-x")) :
                      Helpers::getDefaultVpExe();
    File vpDetourDir = args.containsOption("--vp-detour|-D")              ?
                        File(args.removeValueForOption("--vp-detour|-D")) :
                        Helpers::getDefaultVpDetour();
    File vpHomeDir = args.containsOption("--vp-home|-H")              ?
                      File(args.removeValueForOption("--vp-home|-H")) :
                      Helpers::getDefaultVpHome();
    File vpHelperFile = args.containsOption("--helper-library|-l")           ?
                      File(args.removeValueForOption("--helper-library|-l")) :
                      Helpers::getDefaultVpHelper();
    String keyPath = args.containsOption("--pubkey-file|-k")       ?
                     args.removeValueForOption("--pubkey-file|-k") :
                     "@default.pub";

#if !defined(WIN32) && !defined(_WIN32) && !defined(__APPLE__)
    /* Disable detouring by default on Linux */
    if (!args.removeOptionIfFound("--enable-detour")) {
      vpDetourDir = File();
      vpHomeDir = vpExeFile.getParentDirectory();
    }
#else
    args.removeOptionIfFound("--enable-detour");
#endif

    RSAKey key;
    if (!KeyManager::loadKey(key, keyPath)) {
      Logger::writeToLog("No such pubkey file: " + keyPath);
      return;
    }

    if (!vpExeFile.existsAsFile()) {
      Helpers::msgBox("Can't find Voicepeak executable", SVKEY_N,
                      "ok", "warning");
    }

    if (!vpHelperFile.existsAsFile()) {
      Helpers::msgBox("Can't find YumePatch helper library", SVKEY_N,
                      "ok", "warning");
    }

    if (!vpDetourDir.getFullPathName().isEmpty() &&
        !vpDetourDir.isDirectory() &&
        !vpDetourDir.createDirectory().wasOk()) {
      Helpers::msgBox("Can't create YumePatch detour directory.", SVKEY_N,
                      "ok", "warning");
    }

    if (!vpHomeDir.isDirectory() && !vpHomeDir.createDirectory().wasOk()) {
      Helpers::msgBox("Can't create YumePatch home directory.", SVKEY_N,
                      "ok", "warning");
    }

#if defined(WIN32) || defined(_WIN32)
    /* Install skeleton files like scripts from base Voicepeak data, if needed */
    File vpHomeDirDocsThirdParty = vpHomeDir.getChildFile("docs")
                                            .getChildFile("third-party");

    if ((!vpHomeDirDocsThirdParty.isDirectory()) &&
         Helpers::msgBox("Some files used by Voicepeak are missing. "
                         "Install them?", SVKEY_N, "yes|no",
                         "question") == "yes") {

      File svKeyLicenseDoc = File::getSpecialLocation(
                             File::SpecialLocationType::currentExecutableFile)
                             .getSiblingFile("LICENSE.txt");
      if (svKeyLicenseDoc.existsAsFile() && svKeyLicenseDoc.hasReadAccess()) {
        /* Add our license document to the third party docs directory */
        if (vpHomeDirDocsThirdParty.isDirectory() ||
            vpHomeDirDocsThirdParty.createDirectory()) {
          svKeyLicenseDoc.copyFileTo(vpHomeDirDocsThirdParty
                                     .getChildFile("YumeKey.txt"));
        }
      }
    }
#endif

    YumePatch patcher(vpExeFile, vpDetourDir, vpHelperFile, key, true);

    StringArray passArgsArr;
    for (int i = args[0].text == "--vpeasy" ? 1 : 0; i++; i < args.size()) {
      passArgsArr.add(args[i].text);
    }

    String firstArg = args[0].text == "--vpeasy" ? args[1].text : args[0].text;

    if (firstArg.endsWith(".vppk")) {
      /* Install VPPK */

      File dbDir = vpHomeDir.getChildFile("storage");
      File keysDir = vpHomeDir.getChildFile("keys");
      ArgumentList subArgs("-", StringArray("-",
                                firstArg, "-d", dbDir.getFullPathName(),
                                          "-K", keysDir.getFullPathName()));

      Commands::installVppk(subArgs);
      if (Helpers::msgBox("Installed " + firstArg + "\nLaunch Voicepeak now?",
                          SVKEY_N, "yes|no", "question") != "yes")
        return;

      if (!patcher.launch()) goto failed_launch;
      return;
    }

    if (patcher.launch(passArgsArr))
      return;

failed_launch:
    Helpers::msgBox("Voicepeak failed to start properly.\n\n"
                    "This could be due to a compatibility error with the "
                    "patch or an installed file.", SVKEY_N, "ok", "error");
  }
};

bool Commands::enableProgress = false;

#if defined(WIN32) || defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
  ConsoleApplication app;

  Logger::setCurrentLogger(new StdOutLogger());

  app.addHelpCommand("--help|-h", "Usage:", true);
  app.addVersionCommand("--version|-v", "SynthV Key Tool " VERSION);

  app.addCommand({ "--gen-rsa-key",
                   "--gen-rsa-key",
                   "Generate RSA key",
                   "Generates an RSA keypair compatible with SVKey",
                   Commands::genRsaKey });

  app.addCommand({ "--gen-code",
                   "--gen-code [-s seed] [-V]",
                   "Generate fake activation code",
                   "Generates a random valid but fake activation code for "
                   "SynthV, or Voicepeak if specified, optionally seeding "
                   "the RNG with the specified seed",
                   Commands::genCode });

  app.addCommand({ "--check-code",
                   "--check-code [code] [-V]",
                   "Checks activation code",
                   "Checks if an activation code is valid for SynthV, or for "
                   "Voicepeak if specified",
                   Commands::checkCode });

  app.addCommand({ "--get-device-id",
                   "--get-device-id [-f file-id -c cpu-model]",
                   "Get current device ID",
                   "Generates the device ID for the current device or using "
                   "the specified parameters",
                   Commands::getDeviceId });

  app.addCommand({ "--get-device-info",
                   "--get-device-info",
                   "Get current device information",
                   "Retrieves the information used to generate device IDs",
                   Commands::getDeviceInfo });

  app.addCommand({ "--get-product-id",
                   "--get-product-id [-n name] [-V vendor]",
                   "Get product ID for an arbitrary product",
                   "Generates the product ID for SynthV or using the "
                   "specified parameters",
                   Commands::getProductId });

  app.addCommand({ "--dump-license",
                   "--dump-license license.bin [-k pubkey-file] [--no-crypt]",
                   "Dump license information",
                   "Dump information contained in the specified license.bin "
                   "file, decrypting with the specified public key",
                   Commands::dumpLicense });

  app.addCommand({ "--dump-nofs",
                   "--dump-nofs voice.nofs [-k pubkey-file] [--extra] "
                   "[--even-more-extra]",
                   "Dump NOFS metadata",
                   "Dump metadata contained a NOFS file, including license "
                   "information decrypted with the specified public key",
                   Commands::dumpNofs });

  app.addCommand({ "--fix-nofs",
                   "--fix-nofs voice.nofs out.nofs "
                   "[-k pubkey-file] [-p privkey-file]",
                   "Fix NOFS signature",
                   "Re-signs a NOFS file with a different private key",
                   Commands::fixNofs });

  app.addCommand({ "--activate-nofs",
                   "--activate-nofs voice.nofs out.nofs [-p privkey-file] "
                   "[-f file-id -c cpu-model]",
                   "Activate NOFS file",
                   "Inserts a license into the specified NOFS file, signed "
                   "with the specified private key",
                   Commands::activateNofs });

  app.addCommand({ "--activate-svs",
                   "--activate-svs license.bin [-p privkey-file] "
                   "[-f file-id -c cpu-model]",
                   "Activate SynthV studio",
                   "Saves a license to the specified license.bin file, signed "
                   "with the specified private key. (Deprecated, use "
                   "--activate-any with no options instead)",
                   Commands::activateAny });

  app.addCommand({ "--activate-any",
                   "--activate-any license.bin [-p privkey-file] "
                   "[-f file-id -c cpu-model] [-n name -V vendor]",
                   "Activate any product",
                   "Saves a license to the specified license.bin file, signed "
                   "with the specified private key",
                   Commands::activateAny });

  app.addCommand({ "--install-svpk",
                   "--install-svpk package.svpk [-d svs-databases-path] "
                   "[-k pubkey-file] [-p privkey-file] "
                   "[-f file-id -c cpu-model] [--no-fix] [--no-activate] "
                   "[--extra]",
                   "Install a SVPK package",
                   "Installs the specified package and activates it using the "
                   "specified private key",
                   Commands::installSvpk });

  app.addCommand({ "--install-vppk",
                   "--install-vppk package.vppk [-d vp-storage-path] "
                   "[-p privkey-file] [-f file-id -c cpu-model] [--no-activate] "
                   "[--extra]",
                   "Install a VPPK package",
                   "Installs the specified package and activates it using the "
                   "specified private key",
                   Commands::installVppk });

  app.addCommand({ "--vpeasy",
                   "--vpeasy [-x vp-exe] [-D vp-detour] [-H vp-home] "
                   "[-l helper-library] [-k pubkey-file] "
#if !defined(WIN32) && !defined(_WIN32)
                   "[--enable-detour] "
#endif
                   "[arguments]",
                   "A drop-in replacement for the Voicepeak executable",
                   "This option probably does what you want it to, without "
                   "causing any hassle",
                   Commands::vpEasy });

  app.addDefaultCommand({
                   "--easy",
                   "--easy [-x svs-exe] [-D svs-detour] [-H svs-home] "
                   "[-l helper-library] [-k pubkey-file] "
#if !defined(WIN32) && !defined(_WIN32)
                   "[--enable-detour] "
#endif
                   "[arguments]",
                   "A drop-in replacement for the SynthV Studio executable",
                   "This option probably does what you want it to, without "
                   "causing any hassle",
                   Commands::easy });

  String argv0 = argv[0];
  StringArray argvArr = StringArray(argv + 1, argc - 1);

  ArgumentList args(argv0, argvArr);
  Commands::parseExtraArgs(args);

  return app.findAndRunCommand(args);
}
