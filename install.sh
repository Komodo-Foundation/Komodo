#!/usr/bin/env bash
#
# install.sh — put `komodo` on your PATH.
#
#   ./install.sh              symlink ~/.local/bin/komodo -> this checkout
#   ./install.sh <dir>        symlink <dir>/komodo instead
#
# Builds ./kmc first (via build-self.sh) if it isn't there yet. The
# installed command is a symlink, so `komodo` always resolves to this
# checkout's kmc/stdlib — pull and re-run install.sh only if the symlink
# itself ever needs recreating (rarely: editing the source is enough).
#
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bindir="${1:-$HOME/.local/bin}"

if [[ ! -x "$here/kmc" ]]; then
    echo "no ./kmc yet — building it (this takes a couple of minutes the first time)"
    "$here/build-self.sh"
fi

mkdir -p "$bindir"
target="$bindir/komodo"

if [[ -e "$target" && ! -L "$target" ]]; then
    echo "install.sh: $target already exists and isn't a symlink — remove it or pass a different directory" >&2
    exit 1
fi

ln -sf "$here/komodo" "$target"
chmod +x "$here/komodo"
echo "installed: $target -> $here/komodo"

case ":$PATH:" in
    *":$bindir:"*)
        echo "$bindir is on your PATH — try:  komodo --version"
        ;;
    *)
        echo
        echo "$bindir is not on your PATH yet. Add this to your shell's rc file"
        echo "(~/.bashrc, ~/.zshrc, …) and open a new shell:"
        echo
        echo "    export PATH=\"$bindir:\$PATH\""
        ;;
esac
