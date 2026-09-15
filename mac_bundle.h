#pragma once

// On macOS, Finder-launched .app bundles do not reliably use the project
// directory as the current working directory. JENG CHAT loads runtime assets
// with paths such as "assets/cards/AS.png", so this helper changes the working
// directory to JengChat.app/Contents/Resources before Raylib loads assets.
//
// No-op on Windows/Linux.
void PrepareMacBundleWorkingDirectory();
