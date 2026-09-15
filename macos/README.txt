JENG CHAT macOS bundle files

Add these files to the ROOT of the JENG CHAT project:
  mac_bundle.cpp
  mac_bundle.h
  build_macos.sh
  package_macos.sh

Add this folder to the project root:
  macos/
    Info.plist
    JengChat.icns

IMPORTANT main.cpp change
=========================

1. Near the other local includes, add:

    #include "mac_bundle.h"

2. At the very beginning of main(), BEFORE InitWindow() and before any assets
   are loaded, add:

    PrepareMacBundleWorkingDirectory();

The helper is a no-op on Windows/Linux, so it is safe to keep in the shared
codebase.

Building on a Mac
=================

Install Apple's compiler tools:
    xcode-select --install

Install Homebrew if necessary, then:
    brew install raylib pkg-config

Make scripts executable:
    chmod +x build_macos.sh package_macos.sh

Build + create the .app:
    ./package_macos.sh

Output:
    dist/JengChat.app

Test:
    open dist/JengChat.app

Create a ZIP for another Mac:
    ditto -c -k --sequesterRsrc --keepParent \
      dist/JengChat.app \
      dist/JengChat_macOS.zip

Architecture
============
The script creates a NATIVE build for the Mac that runs it:
  Apple Silicon Mac -> arm64
  Intel Mac         -> x86_64

It is not yet a Universal 2 binary.

Gatekeeper
==========
The script performs only an ad-hoc codesign. A Mac receiving the ZIP may
still warn that the developer cannot be verified. Proper frictionless public
distribution requires an Apple Developer ID signature and notarization.
