JENG CHAT — Build macOS without owning a Mac
================================================

These files let GitHub Actions build JengChat.app on GitHub-hosted macOS
machines.

COPY INTO YOUR JENG CHAT PROJECT
--------------------------------

1. Copy this file into the project root:
       build_macos_ci.sh

2. Copy this workflow while preserving its folders:
       .github/workflows/build-macos.yml

3. Keep the macOS bundle files from the earlier JENG CHAT Mac package:
       mac_bundle.cpp
       mac_bundle.h
       macos/Info.plist
       macos/JengChat.icns

4. main.cpp must contain:
       #include "mac_bundle.h"

   And at the beginning of main(), before InitWindow():
       PrepareMacBundleWorkingDirectory();

UPLOAD TO GITHUB
----------------

Commit/push the project to a GitHub repository.

BUILD
-----

GitHub:
    Repository -> Actions -> "Build JENG CHAT for macOS" -> Run workflow

The workflow makes two downloads:

    JengChat-macOS-Apple-Silicon
    JengChat-macOS-Intel

Each artifact contains a ZIP with JengChat.app.

Apple Silicon is for M1/M2/M3/M4/M5-era Macs.
Intel is for older Intel-based Macs.

NO RAYLIB INSTALL REQUIRED ON FRIEND'S MAC
------------------------------------------

The CI script downloads raylib source and links libraylib.a statically into
JENG CHAT. The friend should not need Homebrew, clang++, raylib, pkg-config,
or any compiler tools.

GATEKEEPER
----------

This build uses an ad-hoc signature, not an Apple Developer ID certificate.
A friend's Mac can still warn that the developer cannot be verified.

For a small private test, they can normally:
    Control-click JengChat.app -> Open -> Open

For smooth public distribution, later add Apple Developer ID signing and
Apple notarization.

TESTING
-------

The workflow runs:
    file ...
    otool -L ...

Check the workflow log. The final executable should not depend on a Homebrew
raylib dylib.
