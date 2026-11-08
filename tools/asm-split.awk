# asm-split.awk — split a Komodo-generated NASM file into independently
# assemblable chunks (build-self.sh assembles them in parallel and caches
# each object by the hash of its body).
#
#   awk -v outdir=DIR [-v K=16] -f tools/asm-split.awk main.km.asm
#
# Writes DIR/c_<i>.body (the code/data of chunk i, used as the cache key)
# and DIR/c_<i>.asm (header + `global` for what the chunk defines + `extern`
# for every other chunk's labels + body).  Text is cut before a function
# label `$name:` whose name hashes to 0 mod K, so a cut point depends only
# on the function's own name: editing one function leaves the other chunks
# byte-identical.  `.L` local labels never cross a cut.  The trailing
# data sections form their own chunk.

BEGIN {
    chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz$"
    if (K == "") K = 16
    mode = "hdr"; n = 0; nl = 0
}
function hash(s,   i, x) {
    x = 0
    for (i = 1; i <= length(s); i++) x = (x * 31 + index(chars, substr(s, i, 1))) % 1000003
    return x
}
function newchunk() {
    if (n > 0) close(outdir "/c_" n ".body")
    n++; lines[n] = 0
}
function emit(line) { print line > (outdir "/c_" n ".body") }
function defined(line,   name) {
    if (line ~ /^[A-Za-z_$][A-Za-z0-9_$]*:/) {
        name = line; sub(/:.*/, "", name)
        owner[name] = n; names[n] = names[n] name "\n"; all[++nl] = name
    }
}
mode == "hdr" {
    if ($0 ~ /^section \.text/) { mode = "text"; newchunk(); emit($0); next }
    if ($0 !~ /^global/) hdr = hdr $0 "\n"
    next
}
mode == "text" {
    if ($0 ~ /^section \./ && $0 !~ /^section \.text/) {
        mode = "data"; newchunk(); emit($0); next
    }
    if ($0 ~ /^\$[A-Za-z0-9_]+:/) {
        nm = $0; sub(/:.*/, "", nm)
        if (lines[n] > 0 && hash(nm) % K == 0) { newchunk(); emit("section .text") }
    }
    defined($0); emit($0); lines[n]++
    next
}
mode == "data" { defined($0); emit($0); next }
END {
    close(outdir "/c_" n ".body")
    for (i = 1; i <= n; i++) {
        f = outdir "/c_" i
        out = f ".asm"
        printf "%s", hdr > out
        split(names[i], mine, "\n")
        for (j in mine) if (mine[j] != "") print "global " mine[j] > out
        for (j = 1; j <= nl; j++) if (owner[all[j]] != i) print "extern " all[j] > out
        while ((getline line < (f ".body")) > 0) print line > out
        close(f ".body")
        print "section .note.GNU-stack noalloc noexec nowrite progbits" > out
        close(out)
    }
    print n
}
