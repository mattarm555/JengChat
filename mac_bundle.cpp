#include "mac_bundle.h"

#ifdef __APPLE__

#include <mach-o/dyld.h>
#include <limits.h>
#include <unistd.h>

#include <string>

void PrepareMacBundleWorkingDirectory()
{
    uint32_t size = PATH_MAX;
    char executablePath[PATH_MAX];

    if (_NSGetExecutablePath(executablePath, &size) != 0)
        return;

    char resolvedPath[PATH_MAX];

    if (realpath(executablePath, resolvedPath) == nullptr)
        return;

    std::string path(resolvedPath);

    // Expected:
    // .../JengChat.app/Contents/MacOS/JengChat
    const std::string marker = "/Contents/MacOS/";

    std::size_t markerPos = path.rfind(marker);

    if (markerPos == std::string::npos)
        return;

    std::string resources =
        path.substr(0, markerPos) +
        "/Contents/Resources";

    chdir(resources.c_str());
}

#else

void PrepareMacBundleWorkingDirectory()
{
    // Nothing to do on non-macOS platforms.
}

#endif
