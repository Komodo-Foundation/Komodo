#!/usr/bin/env bash
#
# install.sh — install the `komodo` command.
#
#   curl -fsSL https://raw.githubusercontent.com/lrperks/Komodo/main/install.sh | bash
#   ./install.sh                  from a checkout: symlink ~/.local/bin/komodo to it
#   ./install.sh --prefix <dir>   install the symlink into <dir> instead
#
# Piped from curl (or run outside a checkout) it clones the repository to
# $KOMODO_HOME (default ~/.komodo) first. Nothing is compiled here: the
# first `komodo` run builds the compiler (~2s) and `komodo upgrade` updates it.
#
set -euo pipefail
repo="${KOMODO_REPO:-https://github.com/lrperks/Komodo.git}"
bindir="$HOME/.local/bin"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) bindir="$2"; shift 2 ;;
        -h|--help) sed -n 2,11p "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) bindir="$1"; shift ;;   # a bare directory, as before
    esac
done

die() { echo "install.sh: $*" >&2; exit 1; }

# Where is the checkout? Next to this script if it is one, else clone.
here=""
if [[ -n "${BASH_SOURCE[0]:-}" && -f "$(dirname "${BASH_SOURCE[0]}")/komodo" && -d "$(dirname "${BASH_SOURCE[0]}")/stdlib" ]]; then
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    home="${KOMODO_HOME:-$HOME/.komodo}"
    command -v git >/dev/null 2>&1 || die "git is required to download Komodo (install git and retry)"
    if [[ -d "$home/.git" ]]; then
        echo "updating $home"
        git -C "$home" pull --ff-only -q || die "could not update $home"
    else
        echo "cloning $repo into $home"
        git clone -q --depth 1 "$repo" "$home" || die "clone failed"
    fi
    here="$home"
fi

# Toolchain: nasm, gcc and the Boehm GC are required; say how to get them.
missing=()
command -v nasm >/dev/null 2>&1 || missing+=(nasm)
command -v gcc  >/dev/null 2>&1 || missing+=(gcc)
ldconfig -p 2>/dev/null | grep -q 'libgc\.so\.1' || missing+=(libgc)
if [[ ${#missing[@]} -gt 0 ]]; then
    echo "missing: ${missing[*]}"
    if   command -v apt-get >/dev/null 2>&1; then echo "  sudo apt install nasm gcc libgc1 libgc-dev"
    elif command -v dnf     >/dev/null 2>&1; then echo "  sudo dnf install nasm gcc gc gc-devel"
    elif command -v pacman  >/dev/null 2>&1; then echo "  sudo pacman -S nasm gcc gc"
    elif command -v brew    >/dev/null 2>&1; then echo "  brew install nasm gcc bdw-gc   (Komodo targets x86-64 Linux)"
    fi
    echo "Install them, then run:  komodo doctor"
fi

mkdir -p "$bindir"
target="$bindir/komodo"
if [[ -e "$target" && ! -L "$target" ]]; then
    die "$target already exists and isn't a symlink — remove it or use --prefix <dir>"
fi
chmod +x "$here/komodo"
ln -sf "$here/komodo" "$target"
echo "installed: $target -> $here/komodo"

case ":$PATH:" in
    *":$bindir:"*)
        echo "try:  komodo new hello && cd hello && komodo" ;;
    *)
        echo
        echo "$bindir is not on your PATH. Add this to ~/.bashrc or ~/.zshrc and open a new shell:"
        echo
        echo "    export PATH=\"$bindir:\$PATH\"" ;;
esac
