#!/usr/bin/env python3
"""Generate import proxies for the OS exports the SDK's frozen proxies lack (see find_defs.py for how they were found).

Usage: gen_proxies.py <dir-with-SymbianSource-clones> <output-dir>   (env SDK=<sdk prefix>)

The linker keeps only the FIRST shared object per soname, so a generated proxy has to REPLACE the SDK's proxy of the
same DLL. Each generated proxy therefore contains: every symbol the SDK proxy already exports (so the runtime keeps
linking) + the extra symbols this port needs. Ordinals come from the original frozen .def files."""
import os, re, subprocess, sys, pathlib

defs_root, out_root = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
sdk = pathlib.Path(os.environ.get("SDK", pathlib.Path.home() / "sdk"))
os.environ["PATH"] = f"{sdk / 'bin'}{os.pathsep}{os.environ.get('PATH', '')}"  # tool's inner CMake finds ld.lld/clang++ via PATH
LIBC = """exit strncpy fputc __optind getopt_long __optarg isthreaded fileno __sfileno feof access chdir ftell strtok sprintf
atoi fstat stat time localtime strdup mkstemp fdopen strerror realpath getcwd strcasecmp strtoul strncasecmp tmpfile
setenv""".split()
# dll -> (def file name, extra symbols as they appear in the DEF (mangled), has an SDK proxy to merge with)
PLAN = {
    "libc":     ("libcu.def",   LIBC, True),
    "libm":     ("libmu.def",   "rint log exp pow".split(), True),
    "euser":    ("euseru.def",  ["_ZN4User6AllocZEi"], True),
    "ws32":     ("ws322u.def",  ["_ZN11RWindowBase22SetRequiredDisplayModeE12TDisplayMode", "_ZN12RWindowGroup9ConstructEm",
                                 "_ZN12RWindowGroupC1Ev", "_ZN7RWindow11BeginRedrawEv", "_ZN7RWindowC1Ev"], True),
    "fbscli":   ("fbscli2u.def", ["_ZN10CFbsBitmap6CreateERK5TSize12TDisplayMode", "_ZN10CFbsBitmapC1Ev",
                                 "_ZNK10CFbsBitmap8LockHeapEi", "_ZNK10CFbsBitmap10UnlockHeapEi",
                                 "_ZNK10CFbsBitmap11DataAddressEv"], False),
    "dfpaeabi": ("dfpaeabiu.def", ["__aeabi_i2f"], False),
}
line_re = re.compile(r"^\s*(\S+)\s+@\s+(\d+)\s+NONAME")

def find_def(name):
    hits = [p for p in defs_root.rglob("*") if p.name.lower() == name and "eabi" in [x.lower() for x in p.parts]]
    # prefer the real file over old_* copies
    return sorted(hits, key=lambda p: len(str(p)))[0] if hits else None

def def_names(path):
    return {m.group(1) for l in path.read_text(errors="replace").splitlines() if (m := line_re.match(l))}

def sdk_exports(dll):
    dso = sdk / "proxies" / dll / f"{dll}.dso"
    if not dso.exists(): return set()
    r = subprocess.run(["readelf", "--dyn-syms", "--wide", str(dso)], capture_output=True, text=True)
    names = set()
    for l in r.stdout.splitlines():
        p = l.split()
        if len(p) >= 8 and p[0].endswith(":") and p[6] != "UND":
            names.add(p[7].split("@")[0])
    return names

ok = True
for dll, (fname, extra, merge) in PLAN.items():
    d = find_def(fname)
    if not d:
        print(f"[{dll}] DEF {fname} not found"); ok = False; continue
    avail = def_names(d)
    missing = [s for s in extra if s not in avail]
    if missing: print(f"[{dll}] NOT in {d.name}: {missing}")
    syms = [s for s in extra if s in avail]
    base = (sdk_exports(dll) & avail) if merge else set()
    print(f"[{dll}] {d.name}: extra={len(syms)} sdk-proxy-exports-kept={len(base)}")
    allsyms = list(dict.fromkeys(syms + sorted(base)))
    if len(allsyms) > 256:
        print(f"[{dll}] WARNING {len(allsyms)} symbols > 256 limit; truncating SDK part"); allsyms = allsyms[:256]
    done = False
    for target in (f"{dll}.dll",):  # the tool only accepts plain .dll/.dso basenames
        outdir = out_root / dll
        cmd = ["symbian", "toolchain", "import-proxy", str(d), "--target-dll", target, "--output", str(outdir),
               "--compiler", str(sdk / "bin" / "clang++"), "--linker", str(sdk / "bin" / "ld.lld")]
        for s in allsyms: cmd += ["--symbol", s]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True)
        except FileNotFoundError as e:
            print(f"[{dll}] cannot run symbian CLI: {e}"); break
        print(f"[{dll}] target-dll={target} rc={r.returncode}\n{(r.stdout + r.stderr)[-1500:]}")
        if r.returncode == 0: done = True; break
    ok &= done

print("\n--- generated artifacts")
for p in sorted(out_root.rglob("*")):
    if p.is_file() and "CMakeFiles" not in p.parts and p.name != "CMakeCache.txt": print(p, p.stat().st_size)

print("\n--- CFbsBitmap data-access related exports in FBSCLI2U.DEF (for the 3 symbols not found)")
d = find_def("fbscli2u.def")
if d:
    names = sorted(def_names(d))
    dem = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
    for n, x in zip(names, dem):
        if "CFbsBitmap" in x and re.search(r"Heap|DataAddress|Stride|ScanLine|DataAccess|Bits|Handle", x): print(" ", n, "->", x)
sys.exit(0 if ok else 1)
