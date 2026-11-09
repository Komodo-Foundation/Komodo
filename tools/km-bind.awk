# km-bind.awk -- the engine behind tools/km-bind.
#
# Input : the output of `gcc -E -dD` for a C header (line markers kept).
# Output: a Komodo binding module (.kmm) on stdout.
#
# Variables (-v): TARGET TDIR SYSTARGET STEM ALSO PREFIX MODNAME LIBNAME
#                 PKGNAME HDR CMD NOSKIP
# Plain POSIX awk (runs under mawk): no gawk extensions.

BEGIN {
    for (i = 0; i < 256; i++) asc[sprintf("%c", i)] = i
    n = split("break continue do else enum extern for fn if import in is let link mut module " \
              "new not and or xor null return struct class case match then throw to true false " \
              "until void while be constructor down while int float string bool f32 i8 i16 i32 " \
              "i64 u8 u16 u32 u64 uint print data copy assert join", kws, " ")
    for (i = 1; i <= n; i++) kwset[kws[i]] = 1
    n = split("__extension__ __restrict __restrict__ restrict volatile __volatile__ _Noreturn " \
              "__noreturn__ register _Thread_local __thread _Atomic _Nonnull _Nullable __nonnull", dr, " ")
    for (i = 1; i <= n; i++) dropset[dr[i]] = 1
    ren["__inline"] = "inline"; ren["__inline__"] = "inline"; ren["__const"] = "const"
    ren["__signed__"] = "signed"; ren["__signed"] = "signed"
    ID = "^[A-Za-z_][A-Za-z_0-9]*$"
    nALSO = split(ALSO, alsoa, ":")
    if (STEM != "") stem_re = "^" STEM "(calls)?([-_][^/]*)?\\.h$"
    cur_ok = 0
}

# ---------------------------------------------------------------- reading
function accepted(f,   base, i) {
    if (f ~ /^</) return 0
    sub(/^\.\//, "", f)
    if (f == TARGET) return 1
    if (!SYSTARGET && TDIR != "" && index(f, TDIR "/") == 1) return 1
    base = f; sub(/^.*\//, "", base)
    if (stem_re != "" && base ~ stem_re) return 1
    for (i = 1; i <= nALSO; i++) if (alsoa[i] != "" && index(f, alsoa[i]) > 0) return 1
    return 0
}

/^# [0-9]+ "/ {
    f = $0; sub(/^# [0-9]+ "/, "", f); sub(/".*$/, "", f)
    cur_ok = accepted(f); next
}

/^#[ \t]*define[ \t]/ {
    s = $0; sub(/^#[ \t]*define[ \t]+/, "", s)
    if (!match(s, /^[A-Za-z_][A-Za-z_0-9]*/)) next
    nm = substr(s, 1, RLENGTH); rest = substr(s, RLENGTH + 1)
    if (!(nm in mdef)) { mord[++nmac] = nm; mdef[nm] = 1 }
    mok[nm] = cur_ok
    if (substr(rest, 1, 1) == "(") {
        mfn[nm] = rest; mbody[nm] = ""
    } else {
        delete mfn[nm]
        sub(/^[ \t]+/, "", rest); sub(/[ \t]+$/, "", rest)
        mbody[nm] = rest
    }
    next
}
/^#[ \t]*undef[ \t]/ {
    s = $0; sub(/^#[ \t]*undef[ \t]+/, "", s); sub(/[ \t].*$/, "", s)
    delete mbody[s]; delete mfn[s]; delete mdef[s]; mok[s] = 0
    next
}
/^#/ { next }

{ tokenize($0, cur_ok) }

function tokenize(line, ok,   t) {
    while (line != "") {
        if (match(line, /^[ \t\r]+/)) { line = substr(line, RLENGTH + 1); continue }
        if (match(line, /^[A-Za-z_][A-Za-z_0-9]*/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^[0-9][A-Za-z_0-9.]*/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^"([^"\\]|\\.)*"/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^'([^'\\]|\\.)*'/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^(\.\.\.|<<|>>|&&|\|\||==|!=|<=|>=|->)/)) t = substr(line, 1, RLENGTH)
        else { t = substr(line, 1, 1); RLENGTH = 1 }
        line = substr(line, RLENGTH + 1)
        addtok(t, ok)
    }
}

function addtok(t, ok) {
    if (skd > 0) { if (t == "(") skd++; else if (t == ")") skd--; return }
    if (skw) { skw = 0; if (t == "(") { skd = 1; return } }
    if (t == "__attribute__" || t == "__attribute" || t == "__asm__" || t == "__asm" || \
        t == "__declspec" || t == "_Alignas" || t == "__alignas") { skw = 1; return }
    if (t in dropset) return
    if (t in ren) t = ren[t]
    nt++; tok[nt] = t; tokok[nt] = ok
}

# ------------------------------------------------------------- statements
function run_statements(   i, t, m, bd, body, fdef, ok, pd, j, a) {
    i = 1
    while (i <= nt) {
        m = 0; delete st; stbody = ""; fdef = 0; pd = 0; ok = tokok[i]
        while (i <= nt) {
            t = tok[i]
            if (t == "{") {
                bd = 0; body = ""
                do {
                    if (tok[i] == "{") bd++
                    else if (tok[i] == "}") bd--
                    if (!(bd == 1 && tok[i] == "{") && bd > 0) body = body " " tok[i]
                    i++
                } while (bd > 0 && i <= nt)
                st[++m] = "{}"; stbody = body
                if (m >= 2 && st[m - 1] == ")") { fdef = 1; break }
                continue
            }
            if (t == ";" && pd == 0) { i++; break }
            if (t == "(") pd++
            else if (t == ")") pd--
            st[++m] = t; i++
        }
        if (m > 0) handle_statement(m, ok, fdef)
    }
}

function handle_statement(m, ok, fdef,   i, j, k, a, hasparen, pd) {
    S_static = 0; S_inline = 0; S_extern = 0
    k = 0
    for (i = 1; i <= m; i++) {
        if (st[i] == "static") S_static = 1
        else if (st[i] == "inline") S_inline = 1
        else if (st[i] == "extern") S_extern = 1
        else a[++k] = st[i]
    }
    delete st
    for (i = 1; i <= k; i++) st[i] = a[i]
    m = k
    CUR_OK = ok
    if (m == 0 || st[1] == "_Static_assert") return
    if (st[1] == "typedef") { do_typedef(m); return }
    pd = 0; hasparen = 0
    for (i = 1; i <= m; i++) {
        if (st[i] == "{}") break
        if (st[i] == "(") { hasparen = 1; break }
    }
    if (hasparen) { do_function(m, ok, fdef); return }
    do_other(m, ok)
}

# ----------------------------------------------------- type specifier parse
# spec(): parse declaration specifiers from a[i..hi]; sets S_base S_c.
# returns the index of the first token after them (the declarator).
function spec(a, i, hi,   t, un, sg, lg, sh, kind, base, cn, seen, tag, key, kw) {
    un = 0; sg = 0; lg = 0; sh = 0; kind = ""; base = ""; cn = 0; seen = 0
    while (i <= hi) {
        t = a[i]
        if (t == "const") { cn = 1; i++; continue }
        if (t == "unsigned") { un = 1; seen = 1; i++; continue }
        if (t == "signed") { sg = 1; seen = 1; i++; continue }
        if (t == "short") { sh = 1; seen = 1; i++; continue }
        if (t == "long") { lg++; seen = 1; i++; continue }
        if (base == "" && kind == "" && (t == "int" || t == "char" || t == "float" || t == "double" || t == "void" || t == "_Bool")) {
            kind = t; seen = 1; i++; continue
        }
        if (base == "" && kind != "" && kind != "char" && t == "int") { i++; continue }
        if (base == "" && !seen && kind == "" && (t == "struct" || t == "union" || t == "enum")) {
            kw = t; i++; tag = ""
            if (i <= hi && a[i] ~ ID) { tag = a[i]; i++ }
            if (i <= hi && a[i] == "{}") {
                if (tag == "") tag = "anon_" (++nanon)
                key = kw ":" tag
                if (kw == "enum") define_enum(key, stbody)
                else { sdef[key] = stbody; sdefd[key] = 1; if (tag ~ /^anon_/) LAST_ANON = key }
                i++
            } else if (tag == "") tag = "anon_" (++nanon)
            base = kw ":" tag
            if (CUR_OK) sacc[base] = 1
            continue
        }
        if (t ~ ID && !seen && kind == "" && base == "") {
            if (t == "_Float32") base = "float"
            else if (t == "_Float64" || t == "_Float32x") base = "double"
            else if (t == "_Float128" || t == "_Float64x" || t == "_Float128x" || t == "__float128" || \
                     t == "__int128" || t == "__int128_t" || t == "__uint128_t" || t ~ /^_Decimal/ || t == "_Complex")
                base = "bad:unsupported type " t
            else if (t == "__builtin_va_list") base = "vlist"
            else if (t == "bool" && !(t in td_base)) base = "bool"
            else base = "td:" t
            i++; continue
        }
        break
    }
    if (base == "") {
        if (!seen) base = "bad:no type"
        else if (kind == "char") base = un ? "uchar" : (sg ? "schar" : "char")
        else if (kind == "float") base = "float"
        else if (kind == "double") base = lg ? "ldouble" : "double"
        else if (kind == "void") base = "void"
        else if (kind == "_Bool") base = "bool"
        else if (sh) base = un ? "ushort" : "short"
        else if (lg >= 2) base = un ? "ullong" : "llong"
        else if (lg == 1) base = un ? "ulong" : "long"
        else base = un ? "uint" : "int"
    }
    S_base = base; S_c = cn
    return i
}

# declr(): parse a declarator from a[i..hi]; sets X_np X_name X_arr X_bf X_fn X_isfn X_bad
function declr(a, i, hi,   j, k, d, np0, rb, rc, rn) {
    X_np = 0; X_name = ""; X_arr = 0; X_bf = 0; X_fn = ""; X_isfn = 0; X_bad = ""
    while (i <= hi) {
        if (a[i] == "*") { X_np++; i++ }
        else if (a[i] == "const") i++
        else break
    }
    if (i <= hi && a[i] == "(" && a[i + 1] == "*") {
        np0 = X_np
        j = i + 2
        while (j <= hi && a[j] == "*") j++
        if (j <= hi && a[j] ~ ID) { X_name = a[j]; j++ }
        if (a[j] == ")" && a[j + 1] == "(") {
            d = 0
            for (k = j + 1; k <= hi; k++) {
                if (a[k] == "(") d++
                else if (a[k] == ")") { d--; if (d == 0) break }
            }
            rb = S_base; rc = S_c
            X_fn = cbsig(a, j + 2, k - 1, rb, np0, rc)
            S_base = rb; S_c = rc
            X_isfn = 1; X_np = 0
            i = k + 1
        } else { X_bad = "unsupported declarator"; i = hi + 1 }
    } else if (i <= hi && a[i] ~ ID) {
        X_name = a[i]; i++
        if (i <= hi && a[i] == "(") {
            X_bad = "function type"
            d = 0
            for (; i <= hi; i++) {
                if (a[i] == "(") d++
                else if (a[i] == ")") { d--; if (d == 0) { i++; break } }
            }
        }
    }
    while (i <= hi && a[i] == "[") {
        X_arr++
        while (i <= hi && a[i] != "]") i++
        i++
    }
    if (i <= hi && a[i] == ":") { X_bf = 1; while (i <= hi && a[i] != ",") i++ }
    return i
}

# cbsig(): Komodo signature text for a function-pointer type.
function cbsig(a, lo, hi, rbase, rnp, rc,   sv_b, sv_c, sv_np, sv_nm, sv_arr, sv_bf, sv_fn, sv_isfn, sv_bad, \
               r, bad, i, j, n, pa, m, d, sig, p, b, cc, lst, nl) {
    sv_b = S_base; sv_c = S_c; sv_np = X_np; sv_nm = X_name; sv_arr = X_arr; sv_bf = X_bf
    sv_fn = X_fn; sv_isfn = X_isfn; sv_bad = X_bad
    NOREG++
    bad = ""
    r = kmt(rbase, rnp, rc, "", "ret")
    if (r == "") bad = K_why
    i = lo
    while (bad == "" && i <= hi) {
        m = 0; d = 0; delete pa
        while (i <= hi && !(a[i] == "," && d == 0)) {
            if (a[i] == "(") d++
            else if (a[i] == ")") d--
            pa[++m] = a[i]; i++
        }
        i++
        if (m == 0) continue
        if (m == 1 && pa[1] == "void") continue
        if (pa[1] == "...") { bad = "variadic"; break }
        j = spec(pa, 1, m); b = S_base; cc = S_c
        j = declr(pa, j, m)
        if (X_isfn) p = "int"
        else p = kmt(b, X_np + (X_arr > 0 ? 1 : 0), cc, "", "cb")
        if (p == "") { bad = K_why; break }
        lst[++nl] = p
    }
    NOREG--
    S_base = sv_b; S_c = sv_c; X_np = sv_np; X_name = sv_nm; X_arr = sv_arr; X_bf = sv_bf
    X_fn = sv_fn; X_isfn = sv_isfn; X_bad = sv_bad
    if (bad != "") return "UNSUPPORTED (" bad ")"
    sig = "fn("
    for (i = 1; i <= nl; i++) sig = sig (i > 1 ? ", " : "") lst[i]
    sig = sig ")"
    if (r != "void") sig = sig ": " r
    return sig
}

# ------------------------------------------------------------ type mapping
function intw(b) {
    if (b == "char" || b == "schar" || b == "uchar") return 1
    if (b == "short" || b == "ushort") return 2
    if (b == "int" || b == "uint" || b ~ /^enum:/) return 4
    return 8
}
function isint(b) {
    return (b == "char" || b == "schar" || b == "uchar" || b == "short" || b == "ushort" || b == "int" || \
            b == "uint" || b == "long" || b == "ulong" || b == "llong" || b == "ullong" || b ~ /^enum:/)
}
function isuns(b) { return (b == "uchar" || b == "ushort" || b == "uint" || b == "ulong" || b == "ullong") }

# kmt(): map a C type (base, pointer depth, const) to a Komodo type.
# role: param | ret | field | cb | var.  "" return = unsupported (K_why).
function kmt(base, np, cn, hn, role,   nm, own, tag, t) {
    K_why = ""; K_kind = ""; K_narrow = 0
    own = np
    while (base ~ /^td:/) {
        nm = substr(base, 4)
        if (!(nm in td_base)) { K_why = "unknown type '" nm "'"; return "" }
        np += td_np[nm]
        if (own == 0 && td_np[nm] == 1 && td_h[nm] != "") hn = td_h[nm]
        if (td_c[nm]) cn = 1
        base = td_base[nm]
    }
    if (own > 0) hn = ""
    if (base ~ /^bad:/) { K_why = substr(base, 5); return "" }
    if (base == "vlist") { K_why = "va_list"; return "" }
    if (base == "ldouble") { K_why = "long double"; return "" }
    if (base ~ /^fnptr:/ && np == 0) {
        if (role == "ret") { if (!NOREG) tmp_ptr = 1; return MODNAME "_Ptr" }
        if (role == "field" || role == "var") { K_why = "function pointer field"; return "" }
        K_kind = "cb"; K_cb = substr(base, 7)
        if (K_cb ~ /^UNSUPPORTED/) { K_why = "callback type: " K_cb; return "" }
        return "int"
    }
    if (np == 0) {
        if (base == "void") { if (role == "ret") return "void"; K_why = "void value"; return "" }
        if (base ~ /^union:/) { K_why = "union by value"; return "" }
        if (base ~ /^struct:/) {
            if (role == "cb") { K_why = "struct by value in callback"; return "" }
            if (role == "field") { K_why = "nested struct field"; return "" }
            if (!struct_ok(base)) { K_why = "struct '" dispname(base) "': " SO_why; return "" }
            if (!NOREG) tmp_bv[base] = 1
            K_kind = "struct"
            return "\001S" base "\002"
        }
        if (role == "field") return fieldtype(base)
        if (isint(base)) { K_narrow = (intw(base) < 8 && !isuns(base)); K_kind = "int"; return "int" }
        if (base == "float") return "f32"
        if (base == "double") return "float"
        if (base == "bool") return "bool"
        K_why = "unsupported type " base; return ""
    }
    if (role == "field") { K_why = "pointer field"; return "" }
    if (np == 1) {
        if (base == "char" || base == "schar" || base == "uchar") {
            K_kind = "str"; K_wr = !cn
            return (role == "ret") ? "string?" : "string"
        }
        if (base == "void") {
            if (hn != "") { if (!NOREG) tmp_vh[hn] = 1; return hn }
            if (role == "ret") { if (!NOREG) tmp_ptr = 1; return MODNAME "_Ptr" }
            K_kind = "voidptr"; return "int"
        }
        if (base ~ /^(struct|union):/ && !(base in sacc)) {
            if (role == "ret") { if (!NOREG) tmp_ptr = 1; return MODNAME "_Ptr" }
            K_kind = "voidptr"; return "int"
        }
        if (base ~ /^(struct|union):/) {
            if (!NOREG) tmp_h[base] = 1
            K_kind = "handle"
            return "\001H" base "\002"
        }
    }
    if (role == "ret") { if (!NOREG) tmp_ptr = 1; return MODNAME "_Ptr" }
    K_kind = "ptr"
    return "int"
}

function fieldtype(b) {
    if (b == "char" || b == "schar") return "i8"
    if (b == "uchar") return "u8"
    if (b == "short") return "i16"
    if (b == "ushort") return "u16"
    if (b == "int" || b ~ /^enum:/) return "i32"
    if (b == "uint") return "u32"
    if (b == "long" || b == "llong") return "int"
    if (b == "ulong" || b == "ullong") return "u64"
    if (b == "float") return "f32"
    if (b == "double") return "float"
    if (b == "bool") return "bool"
    K_why = "unsupported field type " b
    return ""
}
function fsize(t) {
    if (t == "i8" || t == "u8" || t == "bool") return 1
    if (t == "i16" || t == "u16") return 2
    if (t == "i32" || t == "u32" || t == "f32") return 4
    return 8
}

# struct_ok(): can this struct cross by value?  Fills sf_* arrays.
function struct_ok(tag,   body, n, bt, i, j, k, d, m, fs, b, cc, ft, off, sz, al, mx, nf, ok_, saveb, savec, why) {
    if (tag in sok) { SO_why = sowhy[tag]; return sok[tag] }
    if (!(tag in sdefd)) { sok[tag] = 0; sowhy[tag] = SO_why = "incomplete (opaque) struct"; return 0 }
    sok[tag] = 0
    saveb = S_base; savec = S_c
    n = split(sdef[tag], bt, " ")
    i = 1; nf = 0; off = 0; mx = 1; why = ""
    while (i <= n && why == "") {
        m = 0; d = 0; delete fs
        while (i <= n && !(bt[i] == ";" && d == 0)) {
            if (bt[i] == "{" || bt[i] == "}") why = "nested struct/union"
            fs[++m] = bt[i]; i++
        }
        i++
        if (m == 0) continue
        if (why != "") break
        j = spec(fs, 1, m); b = S_base; cc = S_c
        while (j <= m) {
            j = declr(fs, j, m)
            if (X_name == "") { why = "unnamed field"; break }
            if (X_arr > 0) { why = "array field '" X_name "'"; break }
            if (X_bf) { why = "bit-field '" X_name "'"; break }
            if (X_isfn || X_bad != "") { why = "function-pointer field"; break }
            ft = kmt(b, X_np, cc, "", "field")
            if (ft == "") { why = K_why " ('" X_name "')"; break }
            nf++
            sf_name[tag, nf] = X_name; sf_type[tag, nf] = ft
            sz = fsize(ft); al = sz
            off = int((off + al - 1) / al) * al + sz
            if (al > mx) mx = al
            if (j <= m && fs[j] == ",") j++
            else break
        }
    }
    S_base = saveb; S_c = savec
    if (why == "" && nf == 0) why = "no fields"
    if (why == "") {
        off = int((off + mx - 1) / mx) * mx
        if (off > 192) why = "larger than 192 bytes"
    }
    if (why != "") { sowhy[tag] = SO_why = why; return 0 }
    sf_n[tag] = nf; sok[tag] = 1; SO_why = ""
    return 1
}

function dispname(tag,   n) {
    if (tag in sname) return sname[tag]
    n = tag; sub(/^[a-z]+:/, "", n); return n
}
function hname_final(tag) {
    if (tag in ptrtd) return ptrtd[tag]
    if (tag in bv_final) return dispname(tag) "Ptr"
    return dispname(tag)
}

# ------------------------------------------------------------- typedef
function do_typedef(m,   i, j, b, cc, nm, tnp, tb, ok, tagk) {
    ok = CUR_OK
    LAST_ANON = ""
    i = spec(st, 2, m)
    b = S_base; cc = S_c
    while (i <= m) {
        i = declr(st, i, m)
        nm = X_name
        if (nm != "") {
            if (X_isfn) {
                td_base[nm] = "fnptr:" X_fn; td_np[nm] = 0; td_c[nm] = 0; td_h[nm] = ""
                if (ok && pfx_ok(nm) && !(nm ~ /^_/)) cbtypes[++ncb] = nm
            } else if (X_bad != "") {
                td_base[nm] = "bad:" X_bad; td_np[nm] = 0; td_c[nm] = 0; td_h[nm] = ""
            } else {
                td_base[nm] = b; td_np[nm] = X_np + (X_arr > 0 ? 1 : 0); td_c[nm] = cc
                td_h[nm] = ""
                if (X_np == 1 && X_arr == 0) td_h[nm] = nm
                if (X_np == 0 && b ~ /^td:/ ) { }
                # structural facts about this typedef
                tagk = b
                if (tagk ~ /^td:/) { r_ = resolve_tag(tagk); tagk = r_ }
                if (tagk ~ /^(struct|union):/) {
                    if (X_np == 0 && X_arr == 0) {
                        if (!(tagk in sname) || nm == substr(tagk, index(tagk, ":") + 1) ||
                            (substr(tagk, index(tagk, ":") + 1) ~ /^(anon_|_)/ && sname_auto[tagk]))
                            { sname[tagk] = nm; sname_auto[tagk] = (nm != substr(tagk, index(tagk, ":") + 1)) }
                        if (ok && pfx_ok(nm) && !(nm ~ /^_/) && !(tagk in opq_seen)) {
                            opq_seen[tagk] = 1; opq[++nopq] = tagk
                        }
                    } else if (X_np == 1 && X_arr == 0 && b !~ /^td:/ && !(tagk in ptrtd)) {
                        ptrtd[tagk] = nm
                        if (ok && pfx_ok(nm) && !(nm ~ /^_/)) hdecl[++nhd] = tagk
                    }
                } else if (resolves_void(b) && X_np == 1 && X_arr == 0) {
                    if (ok && pfx_ok(nm) && !(nm ~ /^_/)) vhdecl[++nvh] = nm
                }
            }
        }
        if (i <= m && st[i] == ",") i++
        else break
    }
}
function resolve_tag(b,   nm, g) {
    g = 0
    while (b ~ /^td:/ && g++ < 50) {
        nm = substr(b, 4)
        if (!(nm in td_base) || td_np[nm] > 0) return ""
        b = td_base[nm]
    }
    return b
}
function resolves_void(b) {
    return (b == "void")
}
function pfx_ok(nm) { return (PREFIX == "" || index(nm, PREFIX) == 1) }

# ------------------------------------------------------------ enum / other
function define_enum(key, body,   n, items, i, name, expr, val, nxt, known, d, c, parts, np, p, k, cur, started) {
    # split body on top-level commas
    np = 0; cur = ""; d = 0
    n = split(body, items, " ")
    for (i = 1; i <= n; i++) {
        if (items[i] == "(") d++
        else if (items[i] == ")") d--
        if (items[i] == "," && d == 0) { parts[++np] = cur; cur = "" }
        else cur = cur " " items[i]
    }
    if (cur ~ /[^ ]/) parts[++np] = cur
    nxt = 0; known = 1; started = 0
    for (p = 1; p <= np; p++) {
        c = parts[p]; sub(/^ +/, "", c)
        if (c !~ /^[A-Za-z_]/) continue
        name = c; sub(/ .*$/, "", name)
        if (c ~ / = /) {
            expr = c; sub(/^[^=]*= /, "", expr)
            val = ev_top(expr)
            if (EVOK) { known = 1; nxt = val }
            else known = 0
        } else if (!known) val = ""
        else val = nxt
        if (known && EVOK_or_implicit(c)) { cval[name] = nxt; nxt = nxt + 1 }
        else { nxt = nxt + 1 }
        if (CUR_OK && known && name !~ /^_[A-Z_]/ && pfx_ok(name) && !(name in cdone)) {
            if (!started) { cl_n++; cl_kind[cl_n] = "cmt"; cl_val[cl_n] = (key ~ /:anon_/ ? "enum" : "enum " dispname(key)); started = 1 }
            cdone[name] = 1
            cl_n++; cl_kind[cl_n] = "val"; cl_name[cl_n] = name; cl_val[cl_n] = fmtnum(cval[name])
        } else if (CUR_OK && !known && name !~ /^_/ && pfx_ok(name)) {
            sk_add("enum constant " name " (value is not a constant integer expression)")
        }
    }
}
function EVOK_or_implicit(c) { return (c !~ / = / || EVOK) }

function do_other(m, ok,   i, b, cc, ft, nm, sym, line, ty, saveok) {
    LAST_ANON = ""
    i = spec(st, 1, m)
    b = S_base; cc = S_c
    if (i > m) return
    if (!S_extern) return                     # plain variable definition / nothing to bind
    while (i <= m) {
        i = declr(st, i, m)
        nm = X_name
        if (nm != "" && ok && pfx_ok(nm) && nm !~ /^_/) {
            if (X_arr > 0 || X_bad != "" || X_isfn) sk_add("variable " nm " (array or function-pointer variable)")
            else {
                tmp_clear()
                ty = kmt(b, X_np, cc, "", "var")
                if (ty == "" && b ~ /^td:|^struct:|^union:/ && X_np == 0) { }
                if (ty == "" || ty == "void") sk_add("variable " nm " (" K_why ")")
                else if (ty ~ /^\001S/) sk_add("variable " nm " (struct by value)")
                else if (ty == "int" && K_kind == "voidptr" || ty == "int" && K_kind == "ptr")
                    { tmp_ptr = 1; ty = MODNAME "_Ptr"; commit_tmp(); add_var(nm, ty) }
                else if (ty == "int" && K_kind == "int") { add_var(nm, vartype(b, X_np)) }
                else { commit_tmp(); add_var(nm, (ty == "string" ? "string" : ty)) }
            }
        }
        if (i <= m && st[i] == ",") i++
        else break
    }
}
function vartype(b,   nm, g) {
    g = 0
    while (b ~ /^td:/ && g++ < 50) b = td_base[substr(b, 4)]
    return fieldtype(b)
}
function add_var(nm, ty,   kn) {
    if (nm in fdone) return
    fdone[nm] = 1
    kn = nm
    VR_n++
    if (nm in kwset) { kn = nm "_"; VR_line[VR_n] = "extern let " kn ": " ty " = \"" nm "\"" }
    else VR_line[VR_n] = "extern let " kn ": " ty
}

# -------------------------------------------------------------- functions
function tmp_clear() { delete tmp_h; delete tmp_bv; delete tmp_vh; tmp_ptr = 0 }
function commit_tmp(   k) {
    for (k in tmp_h) hused[k] = 1
    for (k in tmp_bv) bv_final[k] = 1
    for (k in tmp_vh) vh_used[k] = 1
    if (tmp_ptr) needptr = 1
}

function sk_add(s) { SK_n++; SK_line[SK_n] = s }

function do_function(m, ok, fdef,   p, q, name, rb, rc, i, j, d, pa, k, nparams, seg, rt, line, notes, why, \
                     pn, ptype, pb, pcc, pnames, dup, comment, wr, ptrs, byv, cbs, narrow, sgn, kn, sym, b1) {
    # `type ( name ) ( params )` (parenthesised name, e.g. libpng) -> `type name ( params )`
    for (i = 2; i + 3 <= m; i++)
        if (st[i] == "(" && st[i + 1] ~ ID && st[i + 2] == ")" && st[i + 3] == "(") {
            nm_ = st[i + 1]
            for (j = i + 1; j + 2 <= m; j++) st[j] = st[j + 2]
            st[i] = nm_
            delete st[m]; delete st[m - 1]; m -= 2
            break
        }
    for (p = 1; p <= m; p++) if (st[p] == "(") break
    if (p < 2 || st[p - 1] !~ ID || st[p - 1] in baseword || st[p + 1] == "*" || st[p + 1] == "(") {
        if (ok && p >= 2 && st[p - 1] ~ ID && st[p - 1] !~ /^_/) sk_add(st[p - 1] " (unsupported declarator)")
        return
    }
    name = st[p - 1]
    if (!ok || name ~ /^_[A-Z_]/ || !pfx_ok(name)) return
    if (name in fdone) return
    if (fdef) { sk_add(name " (inline function body)"); return }
    if (S_static || S_inline) { sk_add(name " (static/inline function)"); return }
    d = 0
    for (q = p; q <= m; q++) {
        if (st[q] == "(") d++
        else if (st[q] == ")") { d--; if (d == 0) break }
    }
    if (q < m) { sk_add(name " (unsupported declarator)"); return }
    # return type
    for (i = 1; i < p - 1; i++) rt[i] = st[i]
    j = spec(rt, 1, p - 2)
    rb = S_base; rc = S_c
    declr(rt, j, p - 2)
    if (X_name != "" || X_bad != "" || X_arr > 0) { sk_add(name " (unsupported declarator)"); return }
    tmp_clear()
    K_cb = ""
    rt_ = kmt(rb, X_np, rc, "", "ret")
    if (rt_ == "") { sk_add(name " (return type: " K_why ")"); return }
    narrow = K_narrow
    # params
    nparams = 0; why = ""; line = ""
    i = p + 1
    while (i < q) {
        k = 0; d = 0; delete pa
        while (i < q && !(st[i] == "," && d == 0)) {
            if (st[i] == "(") d++
            else if (st[i] == ")") d--
            pa[++k] = st[i]; i++
        }
        i++
        if (k == 0) continue
        if (k == 1 && pa[1] == "void") continue
        if (pa[1] == "..." ) { why = "variadic"; break }
        j = spec(pa, 1, k); pb = S_base; pcc = S_c
        j = declr(pa, j, k)
        pn = X_name
        if (X_isfn) { ptype = "int"; K_kind = "cb"; K_cb = X_fn
            if (K_cb ~ /^UNSUPPORTED/) { why = "callback parameter: " K_cb; break } }
        else {
            ptype = kmt(pb, X_np + (X_arr > 0 ? 1 : 0), pcc, "", "param")
            if (ptype == "") { why = "parameter " (pn == "" ? nparams : "'" pn "'") ": " K_why; break }
        }
        nparams++
        sub(/^_+/, "", pn)
        if (pn == "" ) pn = "a" (nparams - 1)
        if (pn in kwset || pn ~ /^[0-9]/) pn = pn "_"
        while (pn in pnames) pn = pn "_"
        pnames[pn] = 1
        line = line (nparams > 1 ? ", " : "") pn ": " ptype
        if (K_kind == "str" && K_wr) wr = wr (wr == "" ? "" : ", ") pn
        else if (K_kind == "ptr" || K_kind == "voidptr") ptrs = ptrs (ptrs == "" ? "" : ", ") pn
        else if (K_kind == "struct") byv = byv (byv == "" ? "" : ", ") pn
        else if (K_kind == "cb") cbs = cbs (cbs == "" ? "" : "; ") pn ": " K_cb
    }
    if (why != "") { sk_add(name " (" why ")"); return }
    fdone[name] = 1
    commit_tmp()
    if (rt_ == "void") {
        line = "extern void " (name in kwset ? name "_" : name) "(" line ")"
    } else {
        line = "extern fn " (name in kwset ? name "_" : name) "(" line "): " rt_
    }
    if (name in kwset) line = line " = \"" name "\""
    F_n++
    F_line[F_n] = line
    comment = ""
    if (narrow) comment = comment "returns a C " (rb ~ /^td:/ ? "narrow integer" : "int") " (no sign extension: negatives may read as 2^32+n); "
    if (ptrs != "") comment = comment "pointer params " ptrs ": pass data(arr) or a handle (64-bit element slots); "
    if (wr != "") comment = comment "non-const char* params " wr ": C may write; Komodo strings are read-only; "
    if (byv != "") comment = comment "struct by value: pass copy(x) for " byv "; "
    if (cbs != "") comment = comment "callback " cbs "; "
    sub(/; $/, "", comment)
    F_note[F_n] = comment
}

# ------------------------------------------------------------- constants
function fmtnum(v) {
    if (v == int(v) && v < 9007199254740992 && v > -9007199254740992) return sprintf("%.0f", v)
    return sprintf("%.17g", v)
}
function ev_top(text,   v) {
    EVOK = 1; EV_unsig = 0
    v = ev_at(text, 1)
    if (en[1] >= ep[1] + 1 && ep[1] <= en[1]) EVOK = 0
    if (EVOK && (v != int(v) || v >= 9007199254740992 || v <= -9007199254740992)) EVOK = 0
    return v
}
function ev_at(text, d,   line, t, n) {
    n = 0; line = text
    while (line != "") {
        if (match(line, /^[ \t]+/)) { line = substr(line, RLENGTH + 1); continue }
        if (match(line, /^0[xX][0-9a-fA-F]+[uUlL]*/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^[0-9]+[uUlL]*/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^[A-Za-z_][A-Za-z_0-9]*/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^'([^'\\]|\\.)'/)) t = substr(line, 1, RLENGTH)
        else if (match(line, /^(<<|>>)/)) t = substr(line, 1, RLENGTH)
        else { t = substr(line, 1, 1); RLENGTH = 1 }
        line = substr(line, RLENGTH + 1)
        et[d, ++n] = t
    }
    en[d] = n; ep[d] = 1
    return e_or(d)
}
function ev_peek(d) { return (ep[d] <= en[d]) ? et[d, ep[d]] : "" }
function e_or(d,   a, b) {
    a = e_xor(d)
    while (ev_peek(d) == "|") { ep[d]++; b = e_xor(d); a = bitop(a, b, "or") }
    return a
}
function e_xor(d,   a, b) {
    a = e_and(d)
    while (ev_peek(d) == "^") { ep[d]++; b = e_and(d); a = bitop(a, b, "xor") }
    return a
}
function e_and(d,   a, b) {
    a = e_shift(d)
    while (ev_peek(d) == "&") { ep[d]++; b = e_shift(d); a = bitop(a, b, "and") }
    return a
}
function e_shift(d,   a, b, o) {
    a = e_add(d)
    while ((o = ev_peek(d)) == "<<" || o == ">>") {
        ep[d]++; b = e_add(d)
        if (b < 0 || b > 52) { EVOK = 0; return 0 }
        if (o == "<<") a = a * (2 ^ b); else a = int(a / (2 ^ b))
    }
    return a
}
function e_add(d,   a, b, o) {
    a = e_mul(d)
    while ((o = ev_peek(d)) == "+" || o == "-") {
        ep[d]++; b = e_mul(d)
        a = (o == "+") ? a + b : a - b
    }
    return a
}
function e_mul(d,   a, b, o) {
    a = e_un(d)
    while ((o = ev_peek(d)) == "*" || o == "/" || o == "%") {
        ep[d]++; b = e_un(d)
        if (o == "*") a = a * b
        else if (b == 0) { EVOK = 0; return 0 }
        else if (o == "/") a = (a / b >= 0) ? int(a / b) : -int(-a / b)
        else a = a - b * ((a / b >= 0) ? int(a / b) : -int(-a / b))
    }
    return a
}
function e_un(d,   t, v) {
    t = ev_peek(d)
    if (t == "-") { ep[d]++; return -e_un(d) }
    if (t == "+") { ep[d]++; return e_un(d) }
    if (t == "~") {
        ep[d]++; v = e_un(d)
        if (EV_unsig) { EVOK = 0; return 0 }
        return -v - 1
    }
    return e_prim(d)
}
function e_prim(d,   t, v, nm, k, ok_, sv) {
    t = ev_peek(d)
    if (t == "") { EVOK = 0; return 0 }
    if (t == "(") {
        # cast?
        k = ep[d] + 1; ok_ = 0
        while (k <= en[d] && (et[d, k] in cast_words || et[d, k] in td_base)) { ok_ = 1; k++ }
        if (ok_ && et[d, k] == ")" && k + 1 <= en[d]) {
            ep[d] = k + 1
            return e_un(d)
        }
        ep[d]++
        v = e_or(d)
        if (ev_peek(d) != ")") { EVOK = 0; return 0 }
        ep[d]++
        return v
    }
    ep[d]++
    if (t ~ /^0[xX]/) {
        if (t ~ /[uU]/) EV_unsig = 1
        return hexval(t)
    }
    if (t ~ /^[0-9]/) {
        if (t ~ /[uU]/) EV_unsig = 1
        sub(/[uUlL]+$/, "", t)
        if (t ~ /^0[0-7]+$/) return octval(t)
        return t + 0
    }
    if (t ~ /^'/) {
        nm = substr(t, 2, length(t) - 2)
        if (length(nm) == 1) return asc[nm]
        EVOK = 0; return 0
    }
    if (t ~ ID) {
        if (t in cval) return cval[t]
        if ((t in mbody) && !(t in mfn) && d < 20 && mbody[t] != t && mbody[t] != "") {
            sv = EV_unsig
            v = ev_at(mbody[t], d + 1)
            if (ep[d + 1] <= en[d + 1]) EVOK = 0
            EV_unsig = sv
            return v
        }
    }
    EVOK = 0
    return 0
}
function hexval(t,   i, v, c) {
    sub(/[uUlL]+$/, "", t); t = tolower(substr(t, 3)); v = 0
    for (i = 1; i <= length(t); i++) v = v * 16 + index("0123456789abcdef", substr(t, i, 1)) - 1
    return v
}
function octval(t,   i, v) {
    v = 0
    for (i = 1; i <= length(t); i++) v = v * 8 + substr(t, i, 1)
    return v
}
function bitop(a, b, op,   r, p, x, y) {
    if (a < 0 || b < 0 || a != int(a) || b != int(b)) {
        if (op == "or" && a == -1) return -1
        EVOK = 0; return 0
    }
    r = 0; p = 1
    while (a > 0 || b > 0) {
        x = a % 2; y = b % 2
        if (op == "or" ? (x || y) : (op == "and" ? (x && y) : (x != y))) r += p
        a = int(a / 2); b = int(b / 2); p *= 2
    }
    return r
}

function emit_constants(   i, nm, body, v, line) {
    # object-like #defines, source order
    for (i = 1; i <= nmac; i++) {
        nm = mord[i]
        if (!(nm in mdef) || !mok[nm]) continue
        if (nm ~ /^_[A-Z_]/ || !pfx_ok(nm) || (nm in cdone)) continue
        if (nm in mfn) {
            nfnmac++
            if (!NOSKIP) { FM_n++; FM_line[FM_n] = "macro " nm substr(mfn[nm], 1, index(mfn[nm], ")")) " (function-like macro)" }
            continue
        }
        body = mbody[nm]
        if (body == "") continue
        line = const_line(nm, body)
        if (line == "") { nskipmac++; continue }
        cdone[nm] = 1
        cl_n++; cl_kind[cl_n] = "def"; cl_name[cl_n] = nm; cl_val[cl_n] = line
    }
}
function const_line(nm, body,   v, b2) {
    b2 = body
    while (b2 ~ /^\(.*\)$/ && balanced(substr(b2, 2, length(b2) - 2))) b2 = substr(b2, 2, length(b2) - 2)
    if (b2 ~ /^"[^"\\]*"$/) return b2
    if (b2 ~ /^[-+]?([0-9]+\.[0-9]*|\.[0-9]+)([eE][-+]?[0-9]+)?[fFlL]?$/) {
        sub(/[fFlL]$/, "", b2); sub(/^\+/, "", b2)
        if (b2 ~ /^-?\./) sub(/\./, "0.", b2)
        if (b2 ~ /\.$/) b2 = b2 "0"
        if (b2 ~ /\.[eE]/) sub(/\./, ".0", b2)
        return b2
    }
    if (body ~ /~/ && body ~ /[0-9][uU]/) return ""
    if (body ~ /"|\./ ) return ""
    v = ev_top(body)
    if (!EVOK) return ""
    return fmtnum(v)
}
function balanced(s,   i, d, c) {
    d = 0
    for (i = 1; i <= length(s); i++) {
        c = substr(s, i, 1)
        if (c == "(") d++
        else if (c == ")") { d--; if (d < 0) return 0 }
    }
    return d == 0
}

# ---------------------------------------------------------------- output
function fix(s,   out, nm, tag, k) {
    out = ""
    while (match(s, /\001[HS][^\002]*\002/)) {
        out = out substr(s, 1, RSTART - 1)
        tag = substr(s, RSTART + 2, RLENGTH - 3)
        if (substr(s, RSTART + 1, 1) == "H") nm = hname_final(tag)
        else nm = dispname(tag)
        out = out nm
        s = substr(s, RSTART + RLENGTH)
    }
    return out s
}

END {
    cast_words["int"] = cast_words["unsigned"] = cast_words["long"] = cast_words["short"] = cast_words["char"] = cast_words["signed"] = 1
    baseword["int"] = baseword["void"] = baseword["char"] = baseword["short"] = baseword["long"] = baseword["float"] = baseword["double"] = baseword["unsigned"] = baseword["signed"] = baseword["struct"] = baseword["union"] = baseword["enum"] = baseword["_Bool"] = baseword["const"] = 1
    run_statements()
    emit_constants()

    # names taken by functions/variables win over same-named constants (checked below)
    # -------- header
    print "// " MODNAME ".kmm -- Komodo bindings for " HDR
    print "//"
    print "// GENERATED by tools/km-bind; do not edit by hand (re-run the command instead)."
    print "//   command: " CMD
    print "//"
    print "// Conventions used in this file:"
    print "//   * Komodo `int` is 64-bit. C int/long/size_t/uint*_t/int*_t/char map to it. A C"
    print "//     `int` RETURN is not sign-extended (negative results can read as 2^32+n);"
    print "//     such functions carry a note. Arguments are fine."
    print "//   * double -> float, float -> f32, bool -> bool, const char * -> string"
    print "//     (a returned char * is `string?`, test it against null)."
    print "//   * pointers to structs are opaque handles (`extern type`), compared with"
    print "//     `== 0` for NULL; `void *` is `<module>_Ptr` when returned, `int` when passed"
    print "//     (so it accepts any handle or data(arr))."
    print "//   * scalar pointers (int *, double *, ...) are `int`: pass data(arr); the"
    print "//     array holds 64-bit slots, so only 64-bit C element types line up."
    print "//   * structs by value are mirrored as Komodo structs; pass copy(x)."
    print "//   * callbacks (function pointers) are passed as a bare Komodo function name."
    print ""
    print "module " MODNAME
    print ""
    if (LIBNAME != "") {
        if (LIBNAME ~ ID) print "link " LIBNAME
        else print "link \"" LIBNAME "\""
    }
    if (PKGNAME != "") print "link \"pkg:" PKGNAME "\""
    if (LIBNAME != "" || PKGNAME != "") print ""

    # -------- handles
    nh = 0
    if (needptr) hl[++nh] = MODNAME "_Ptr"
    for (k = 1; k <= nhd; k++) { t = hdecl[k]; if (!(t in bv_final) || t in hused) addh(hname_final(t)) }
    for (k = 1; k <= nvh; k++) addh(vhdecl[k])
    for (k = 1; k <= nopq; k++) {
        t = opq[k]
        if (t in ptrtd) continue
        if (!(t in bv_final)) addh(hname_final(t))
        else if (t in hused) addh(hname_final(t))
    }
    for (t in hused) usedlist[++nu] = t
    sortarr(usedlist, nu)
    for (k = 1; k <= nu; k++) addh(hname_final(usedlist[k]))
    for (t in vh_used) vlist2[++nv2] = t
    sortarr(vlist2, nv2)
    for (k = 1; k <= nv2; k++) addh(vlist2[k])
    if (nh > 0) {
        print "// ---- opaque handles"
        for (k = 1; k <= nh; k++) print "extern type " hl[k]
        print ""
    }
    # -------- structs
    nb = 0
    for (t in bv_final) bvl[++nb] = t
    sortarr(bvl, nb)
    if (nb > 0) {
        print "// ---- structs passed by value (pass copy(x))"
        for (k = 1; k <= nb; k++) {
            t = bvl[k]
            print "struct " dispname(t)
            for (j = 1; j <= sf_n[t]; j++) {
                ty = sf_type[t, j]; fnm = sf_name[t, j]
                dv = (ty == "f32" || ty == "float") ? "0.0" : (ty == "bool" ? "false" : "0")
                if (fnm in kwset) fnm = fnm "_"
                print "    " fnm ": " ty " = " dv
            }
            print ""
        }
    }
    # -------- callback typedef notes
    if (ncb > 0) {
        print "// ---- callback types (pass a bare Komodo function with this signature)"
        for (k = 1; k <= ncb; k++) {
            nm = cbtypes[k]
            print "// " nm ": " fix(substr(td_base[nm], 7))
        }
        print ""
    }
    # -------- constants
    taken = ""
    for (k = 1; k <= F_n; k++) { s = F_line[k]; sub(/^extern (fn|void) /, "", s); sub(/\(.*$/, "", s); takenset[s] = 1 }
    for (k = 1; k <= nb; k++) takenset[dispname(bvl[k])] = 1
    for (k = 1; k <= nh; k++) takenset[hl[k]] = 1
    for (k = 1; k <= VR_n; k++) { s = VR_line[k]; sub(/^extern let /, "", s); sub(/[: ].*$/, "", s); takenset[s] = 1 }
    ncst = 0
    for (k = 1; k <= cl_n; k++) {
        if (cl_kind[k] == "cmt") { pend = "// " cl_val[k]; continue }
        if (cl_name[k] in takenset) continue
        if (cl_name[k] in kwset) { sk_add("constant " cl_name[k] " (Komodo keyword)"); continue }
        if (!ncst) print "// ---- constants"
        ncst++
        if (pend != "") { print pend; pend = "" }
        print "let " cl_name[k] " be " cl_val[k]
    }
    if (ncst) print ""
    # -------- globals
    if (VR_n > 0) {
        print "// ---- global variables"
        for (k = 1; k <= VR_n; k++) print fix(VR_line[k])
        print ""
    }
    # -------- functions
    if (F_n > 0) {
        print "// ---- functions"
        for (k = 1; k <= F_n; k++) {
            if (F_note[k] != "") print "// " fix(F_note[k])
            print fix(F_line[k])
        }
        print ""
    }
    if (SK_n > 0 || FM_n > 0 || nskipmac > 0) {
        print "// ---- skipped"
        for (k = 1; k <= SK_n; k++) print "// skipped: " SK_line[k]
        for (k = 1; k <= FM_n; k++) print "// skipped: " FM_line[k]
        if (nskipmac > 0) print "// skipped: " nskipmac " object-like macros that are not integer/float/string constants"
    }
    if (F_n == 0 && ncst == 0 && nb == 0 && nh == 0) print "// (nothing bindable found in this header)"
}

function addh(nm,   i) {
    for (i = 1; i <= nh; i++) if (hl[i] == nm) return
    hl[++nh] = nm
}
function sortarr(a, n,   i, j, t) {
    for (i = 2; i <= n; i++) {
        t = a[i]
        for (j = i - 1; j >= 1 && a[j] > t; j--) a[j + 1] = a[j]
        a[j + 1] = t
    }
}
