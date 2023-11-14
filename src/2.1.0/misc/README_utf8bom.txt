=== YumeKey Tool 2.0 ===

We highly recommend purchasing a license for any products that you are using
with this software.

Xi Jinpwned Software does not condone the commercial usage of this software.
Xi Jinpwned Software is NOT RESPONSIBLE for your usage of this software AT ALL!

YUMEKEY TOOL IS AND ALWAYS WILL BE FREE. IF YOU PAID FOR THIS, YOU'VE BEEN
SCAMMED.

Website: https://jinpwnsoft.re
Contact Email: <support@jinpwnsoft.re>
PGP Public Key: jinpwnsoft.pub

***THERE ARE KNOWN BUGS! PLEASE SEE THE BUGS SECTION!***

--- Setup Guide for SynthV ---

1. Ensure you have a copy of SynthV Studio **Pro** installed.

2. Unzip these files (this README.txt and everything with it) to any folder
   you want. You will need to access this folder repeatedly, so make it
   memorable.

3. Launch YumeKey:

   * on Windows: run SVKey.exe
     (you may want to make a desktop shortcut)

   * on Linux and macOS: run ./svkey from a terminal
     (you may have to pass arguments pointing it to your SynthV Pro
      installation directory/executable)

4. You're done. If you're prompted to activate SynthV Studio, choose the
   yes option.

--- Setup Guide for Voicepeak ---

1. Unzip these files as in the SynthV Setup Guide.

2. Ensure you have Voicepeak installed. On Windows, you may run
   InstallVoicepeak.cmd. On macOS and Linux, you may run the script
   install-voicepeak.sh.

3. Launch YumeKey VP

   * on Windows: run StartVoicepeak.cmd

   * on Linux and macOS: run ./svkey --vpeasy from a terminal

--- Bugs ---

This is still beta software, and there are some known bugs:

* "Detouring" is disabled by default on Linux due to strange behavior.

* On Linux, you may have to copy important directories like fonts/ and dicts/
  to the SVPatchDetour/ directory if you wish to use detouring.

* You must close SynthV before installing SVPK files with SVKey.

* Only Windows binaries are provided. Linux and macOS users must compile from
  the provided source code.

* Installing SVPK files requires loading the entire file into memory. Please
  ensure you have sufficient free memory (~500 MB).

--- FAQ ---

Q: How do I update SVKey?

A: Simply follow the setup guide and begin using the new SVKey. You do not need
   to remove previous versions of SVKey, as long as you keep them separate from
   the new SVKey files. You also do not need to uninstall/reinstall SynthV
   Studio Pro.

Q: How do I install voice database packages (SVPK)?

A: You need to pass the SVPK file path to SVKey:

* on Windows: drag the SVPK file onto the SVKey.exe program (NOT window, but
  the SVKey.exe file as shown in File Explorer)

* on Linux and macOS: run ./svkey path/to/voice.svpk

Q: How do install Voicepeak voices?

A: You must have obtained the voice as a VPPK file, specially repackaged.

* on Windows: drag the VPPK file onto the StartVoicepeak.cmd program

* on Linux and macOS: run ./svkey --vpeasy path/to/voice.vppk

Q: Where will the voice databases, license, etc. be saved?

A: By default, SVKey and patched SynthV Studio will save data to:

* on Windows: %USERPROFILE%\Documents\SVPatchDetour
  (the SVPatchDetour folder in your Documents folder)

   If detouring is enabled on these platforms with --enable-detour, then

* on Linux: $XDG_DOCUMENTS_DIR/SVPatchDetour or ~/Documents/SVPatchDetour

* on macOS: $HOME/SVPatchDetour (TODO)

   Please note that detouring on Linux and macOS is disabled by default due to
   bugs.

Q: Why doesn't checking for updates work?

A: SVPatch prevents communication with the activation server. Please check for
   and install updates manually.

Q: I can't install voices/SVPK files.

A: You cannot install SVPK files from within the editor now. Please see
   "How do I install voice database packages (SVPK)?" in the FAQ at the top.

Q: How do I uninstall SVKey/SVPatch?

A: Simply delete the folder it is saved in. If you want to remove other data,
   see "Where will the voice databases, license, etc. be saved?" in the FAQ and
   remove the relevant folder.

Q: My antivirus/security snakeoil solution says SVKey/SVPatch is a virus!

A: SVKey must perform operations such as DLL injection that normally only
   viruses do, but it is necessary in order to install the patch. You should
   add "SVKey.exe" and "LibSVPatch.dll" to the whitelist.

Q: I installed SynthV Studio in a different (non-default) directory. How do I
   use SVKey?

A: You need to tell SVKey the path to the SynthV Studio executable using the
   -x command line option:

   svkey -x path/to/synthv-studio.exe

   On Windows, you can make this easy by creating a shortcut to SVKey.exe and
   adding the extra option at the end like so:

   (SVKey.exe path) -x "C:\Put\Path\Here\To\synthv-studio.exe"

--- Developers ---

Source code is provided in the source/ directory with all dependencies. You will
need CMake installed to build.

SVKey also has many command line options you can see by passing --help as an
argument.

The NOFS file format is partially documented by the ImHex pattern provided in
the misc/ directory.

A Python library for the Voicepeak activation API is provided as misc/vpapi.py.

A JavaScript library for generating activation codes compatible with SynthV and
Voicepeak is provided as misc/libdtauth.js.
