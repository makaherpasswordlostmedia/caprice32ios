#!/usr/bin/env python3
"""Discovery: which original Symbian .def files export the symbols the link step is missing?
Usage: find_defs.py <dir-with-cloned-SymbianSource-repos>
Prints, per .def file, every needed symbol it exports (ordinal, mangled and demangled form)."""
import sys, re, subprocess, pathlib

root = pathlib.Path(sys.argv[1])
NEEDED = """exit strncpy fputc __optind getopt_long __optarg isthreaded fileno __sfileno feof access chdir ftell strtok
sprintf atoi fstat stat time localtime strdup mkstemp fdopen strerror realpath getcwd strcasecmp strtoul strncasecmp
tmpfile setenv rint log exp pow __aeabi_i2f
User::AllocZ(int) RWindowGroup::Construct(unsigned long) RWindowBase::SetRequiredDisplayMode(TDisplayMode)
CFbsBitmap::CFbsBitmap() CFbsBitmap::Create(TSize const&, TDisplayMode) CFbsBitmap::LockHeap(int) const
CFbsBitmap::DataAddress() const CFbsBitmap::UnlockHeap(int) const RWindow::BeginRedraw()
RWindowGroup::RWindowGroup() RWindow::RWindow()""".replace("\n", " ")
# split on spaces that are not inside parentheses
needed, cur, depth = [], "", 0
for ch in NEEDED:
    if ch == "(": depth += 1
    if ch == ")": depth -= 1
    if ch == " " and depth == 0:
        if cur: needed.append(cur); cur = ""
    else:
        cur += ch
if cur: needed.append(cur)
needed = set(needed)

defs = [p for p in root.rglob("*") if p.suffix.lower() == ".def" and "eabi" in [x.lower() for x in p.parts]]
print(f"{len(defs)} EABI .def files found under {root}")
line_re = re.compile(r"^\s*(\S+)\s+@\s+(\d+)\s+NONAME")
for p in sorted(defs):
    try:
        lines = p.read_text(errors="replace").splitlines()
    except OSError:
        continue
    entries = [(m.group(1), m.group(2)) for l in lines if (m := line_re.match(l))]
    if not entries:
        continue
    names = [e[0] for e in entries]
    dem = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
    hits = [(o, n, d) for (n, o), d in zip(entries, dem) if n in needed or d in needed]
    if hits:
        print(f"\n=== {p.relative_to(root)}  ({len(entries)} exports, {len(hits)} needed)")
        for o, n, d in hits:
            print(f"  @{o:>5}  {n}  ->  {d}")
