"""Generate platform/plugin/src/mechanism/copied/*.cpp from the patched draft tree (T066 spike; moved for Feature 002 T002;
regenerated from patches 0001-0013 for Feature 002 T052).

Usage: python3 -I platform/plugin/gen_copies.py platform/plugin/src/mechanism/copied

Each output file mirrors one upstream translation unit: its includes and using-directives, a spike subclass of the
upstream class whose member functions are the patched function bodies (verbatim apart from the renames listed in the
file header), trampolines that createFunctionHook can point at, and a registration function with the exact demangled
signature of each hooked symbol in the stock binary. A function listed as copy-only is not hooked: the file exports
HyprVXSpike::copied_<name>(self, ...) instead, which a hand-written wrapper (RenderHooks.cpp) calls in place of the
original.
"""
import os
import re
import subprocess
import sys

DRAFT = os.path.expanduser("~/.cache/hyprvx/claude/src/hyprland-draft/")
STOCK_BIN = os.path.expanduser("~/.cache/hyprvx/claude/prefix/stock/bin/Hyprland")
PREFIX_INCLUDE = os.path.expanduser("~/.cache/hyprvx/claude/prefix/stock/include/hyprland")
OUT = sys.argv[1]
DRAFT_COMMIT = subprocess.run(["git", "-C", DRAFT, "rev-parse", "--short=8", "HEAD"], capture_output=True, text=True).stdout.strip()
PATCH_COUNT = int(subprocess.run(["git", "-C", DRAFT, "rev-list", "--count", "efb50993780079460b0cbed1363e2166a2de1d9f..HEAD"],
                                 capture_output=True, text=True).stdout.strip())
NM = subprocess.run(["nm", "-DC", "--defined-only", STOCK_BIN], capture_output=True, text=True).stdout.splitlines()
DEMANGLED = [l.split(" ", 2)[2] for l in NM if " T " in l]

# file, base class (qualified), spike class, functions (unqualified names; overloads all taken), extra static helpers,
# copy-only functions (optional)
SPECS = [
    ("src/render/ElementRenderer.cpp", "Render::IElementRenderer", "SpikeElementRenderer", ["drawSurface"], []),
    ("src/render/OpenGL.cpp", "Render::GL::CHyprOpenGLImpl", "SpikeOpenGL", ["renderBorder"], ["globalFeatures", "getDecoFeatures"]),
    # renderWorkspaceWindows (0010 per-window offset hint) is copy-only: the wrapper in RenderHooks.cpp keeps the pass
    # camera hint around it and calls the copy instead of the original.
    ("src/render/Renderer.cpp", "Render::IHyprRenderer", "SpikeRenderer", ["damageSurface", "renderWorkspaceWindows"], [], ["renderWorkspaceWindows"]),
    ("src/desktop/view/Window.cpp", "Desktop::View::CWindow", "SpikeWindow", ["checkInputOnDecos", "visibleOnMonitor", "warpCursor"], []),
    ("src/desktop/state/ViewHitTester.cpp", "Desktop::CViewHitTester", "SpikeViewHitTester", ["windowAt", "windowSurfaceAt", "surfaceLocalAt"], []),
    ("src/managers/input/InputManager.cpp", "CInputManager", "SpikeInputManager",
     ["mouseMoveUnified", "processMouseDownNormal", "onMouseWheel", "sendMotionEventsToFocused"], []),
    ("src/layout/supplementary/DragController.cpp", "Layout::Supplementary::CDragStateController", "SpikeDragController",
     ["dragBegin", "updateDragWindow", "mouseMove"], ["isResizeMode", "cursorForResizeEdge", "setXDGResizingState"]),
    ("src/desktop/view/Popup.cpp", "Desktop::View::CPopup", "SpikePopup", ["onMap", "reposition"], []),
]


# Code a hook must run where the patch changed a function the spike does not hook. Inserted as the first statements of
# the copied function, and listed in the generated file header.
PROLOGUES = {
    # 0002 preDrawSurface: the camera adjustment of the clip box, which preDrawSurface sets just before calling
    # drawSurface (its only caller) and resets right after it.
    "drawSurface": """    if UNLIKELY (HyprVXSpike::isCameraModif(g_pHyprRenderer->m_renderData.renderModif) && !g_pHyprRenderer->m_renderData.clipBox.empty()) {
        g_pHyprRenderer->m_renderData.renderModif.applyToBox(g_pHyprRenderer->m_renderData.clipBox);
        g_pHyprRenderer->m_renderData.clipBox = HyprVXSpike::roundOutBox(g_pHyprRenderer->m_renderData.clipBox);
    }""",
    # 0007/0011 updateRelativeCursorCoords is a static function (not in the dynamic symbol table, so not hookable). Stock
    # stores screen - window position; convert it to world - O(w) - window position as the patch stores it.
    "warpCursor": """    if (const auto PMON = m_monitor.lock(); PMON && m_relativeCursorCoordsOnLastWarp.x > 0 && m_relativeCursorCoordsOnLastWarp.y > 0)
        m_relativeCursorCoordsOnLastWarp =
            HyprVXSpike::screenToWorld(*PMON, m_relativeCursorCoordsOnLastWarp + layoutBox().pos()) - HyprVXSpike::appliedOffset(*this) - layoutBox().pos();""",
}


def split_top(s, sep=","):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "<([{":
            depth += 1
        elif ch in ">)]}":
            depth -= 1
        if ch == sep and depth == 0:
            out.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [p.strip() for p in out]


def functions(lines, cls, name):
    """Yield (header_text, body_lines) for each top-level definition of cls::name."""
    i = 0
    pat = re.compile(r"^[A-Za-z].*\b" + re.escape(cls) + "::" + re.escape(name) + r"\(")
    while i < len(lines):
        if pat.match(lines[i]):
            j = i
            header = lines[i]
            while "{" not in lines[j]:
                j += 1
                header += " " + lines[j].strip()
            k = j
            while lines[k] != "}":
                k += 1
            yield header, lines[i:k + 1]
            i = k
        i += 1


def static_helper(lines, name):
    for i, l in enumerate(lines):
        if re.match(r"^static .*\b" + re.escape(name) + r"\(", l):
            k = i
            while lines[k] != "}":
                k += 1
            return lines[i:k + 1]
    raise SystemExit(f"helper {name} not found")


def convert_include(line, srcdir):
    m = re.match(r'#include\s+"([^"]+)"', line)
    if not m:
        return line
    rel = m.group(1)
    if "hyprvx/" in rel:
        return None
    path = os.path.normpath(os.path.join(srcdir, rel))
    if not os.path.exists(os.path.join(DRAFT, path)):
        path = os.path.normpath(os.path.join("src", rel)) if os.path.exists(os.path.join(DRAFT, "src", rel)) else rel
    if not os.path.exists(os.path.join(PREFIX_INCLUDE, path)):
        return f"// not installed with the stock headers (build-time file): {line.strip()}"
    return f"#include <{path}>"


def transform(text):
    text = text.replace("m_renderData.renderModif.enabled && m_renderData.renderModif.hyprvxCamera", "HyprVXSpike::isCameraModif(m_renderData.renderModif)")
    text = text.replace("HyprVX::", "HyprVXSpike::")
    return text


def demangled_for(base, name, nparams, const):
    want = base + "::" + name + "("
    cands = [d for d in DEMANGLED if d.startswith(want) and "[clone" not in d]
    good = []
    for d in cands:
        inner = d[len(want):d.rindex(")")]
        n = len(split_top(inner)) if inner.strip() else 0
        if n == nparams and d.endswith(" const") == const:
            good.append(d)
    if len(good) != 1:
        raise SystemExit(f"demangled match for {want} ({nparams} params, const={const}): {good or cands}")
    return good[0]


for path, base, spike, names, helpers, *rest in SPECS:
    copy_only = set(rest[0]) if rest else set()
    lines = open(DRAFT + path).read().split("\n")
    srcdir = os.path.dirname(path)
    cls = base.split("::")[-1]
    # The preprocessor preamble (includes with their #if guards), up to the first line of code.
    preamble = []
    for l in lines:
        if l.strip() and not l.startswith(("#", "//", "/*", " *")):
            break
        preamble.append(l)
    includes = [c for c in (convert_include(l, srcdir) for l in preamble) if c is not None]
    usings = [l for l in lines if l.startswith("using namespace")]
    decls, defs, tramps, regs, exports = [], [], [], [], []
    for name in names:
        found = list(functions(lines, cls, name))
        if not found:
            raise SystemExit(f"{path}: {cls}::{name} not found")
        for idx, (header, body) in enumerate(found):
            sig = header[:header.index("{")].strip()
            const = sig.endswith("const")
            pre, rest = sig.split(cls + "::" + name + "(", 1)
            params = rest[:rest.rindex(")")]
            ret = pre.strip()
            plist = split_top(params) if params.strip() else []
            pnames = [re.findall(r"[A-Za-z_]\w*", p)[-1] for p in plist]
            decls.append(f"        {ret} {name}({params}){' const' if const else ''};")
            body = list(body)
            body[0] = body[0].replace(cls + "::" + name + "(", spike + "::" + name + "(", 1)
            if name in PROLOGUES:
                brace = next(i for i, l in enumerate(body) if "{" in l)
                body.insert(brace + 1, "    // HyprVX spike prologue (gen_copies.py PROLOGUES)\n" + PROLOGUES[name])
            defs.append(transform("\n".join(body)))
            hk = f"hk_{name}{idx if len(found) > 1 else ''}"
            selftype = f"{'const ' if const else ''}{base}*"
            call = f"static_cast<{'const ' if const else ''}{spike}*>(self)->{name}({', '.join(pnames)})"
            if name in copy_only:
                exports.append(f"{ret} HyprVXSpike::copied_{name}({selftype} self{', ' if plist else ''}{', '.join(plist)}) {{\n    {'return ' if ret != 'void' else ''}{call};\n}}")
                continue
            # Trampolines are static members so that nested types in the parameter list (e.g. SBorderRenderData) resolve.
            tramps.append(f"    static {ret} {hk}({selftype} self{', ' if plist else ''}{', '.join(plist)}) {{\n        {'return ' if ret != 'void' else ''}{call};\n    }}")
            regs.append(f'    specs.push_back({{"{name}", "{demangled_for(base, name, len(plist), const)}", reinterpret_cast<void*>(&{spike}::{hk}), nullptr}});')
    for h in reversed(helpers):
        defs.insert(0, "\n".join(static_helper(lines, h)))
    unit = os.path.splitext(os.path.basename(path))[0]
    out = [
        f"// Generated for the HyprVX plugin form S (Feature 002 T052; delivery spike T066) from the patched draft tree",
        f"// (hyprland-draft {DRAFT_COMMIT} = Hyprland efb5099 + patches 0001-{PATCH_COUNT:04d}), {path}. Bodies are verbatim apart from: the class renamed to the spike subclass,",
        "// HyprVX:: -> HyprVXSpike::, and the SRenderModifData::hyprvxCamera test -> HyprVXSpike::isCameraModif().",
        "// Prologues from gen_copies.py PROLOGUES: " + (", ".join(n for n in names if n in PROLOGUES) or "none") + ".",
        "// Copy-only (not hooked; called by a wrapper as HyprVXSpike::copied_<name>): " + (", ".join(sorted(copy_only)) or "none") + ".",
        "// Upstream code: Hyprland, BSD-3-Clause (docs/THIRD_PARTY.md). Do not edit by hand; regenerate with",
        "// python3 -I platform/plugin/gen_copies.py platform/plugin/src/mechanism/copied",
        '#include "../SpikeAccess.hpp"',
        "",
        *includes,
        "",
        '#include "../Camera.hpp"',
        '#include "../Hooks.hpp"',
        "",
        *usings,
        "",
        f"struct {spike} : public {base} {{",
        *decls,
        "",
        *tramps,
        "};",
        "",
        *defs,
        "",
        *exports,
        "",
        f"void HyprVXSpike::addCopied{unit}Hooks(std::vector<SHookSpec>& specs) {{",
        *regs,
        "}",
        "",
    ]
    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, unit + ".cpp"), "w").write("\n".join(out))
    print(f"{unit}.cpp: {sum(len(d.splitlines()) for d in defs)} copied lines, {len(regs)} hooks, {len(exports)} copy-only")
