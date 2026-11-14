#!/usr/bin/env bash
# Local real JUCE capture. Persistent cache survives model chats.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "$0")" && pwd)"
exec python3 "$ROOT/tools/quick4_workspace.py" --plugin skell-preset-vst "$@"
