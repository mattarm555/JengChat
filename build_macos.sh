#!/bin/bash
set -euo pipefail

# Build the same Universal app locally that CI produces.
# The resulting JengChat.app contains both arm64 and x86_64.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "${SCRIPT_DIR}/build_macos_ci.sh"
