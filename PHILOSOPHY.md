# Framework philosophy

This document is the **source of truth** for design principles in this repository. Tools and editors (for example Cursor) should follow it when generating or changing code and config. For a short overview and documentation map, see [README.md](README.md). For step-by-step setup, see [INSTALL.md](INSTALL.md).

## Purpose

This framework standardizes **PicoDst-based analyses** at STAR using the familiar **StChain / StMaker** pattern: the event loop and I/O live in the chain; physics, cuts (via config), and histograms live in compiled Makers. Configuration is **YAML-driven** with a single **mainconf** entry point so that runs are easier to reproduce, review, and automate—including with AI-assisted development.

## Design principles

- **No custom event loop in macros.** The macro builds a `StChain`, calls `chain->Init()`, then runs `chain->Make(i)` in a loop and `chain->Finish()`. The framework controls the loop. *Why:* one place to reason about event flow and STAR/ROOT integration; avoids divergent “home-grown” loops per analysis.

- **Physics in StMaker subclasses.** Each analysis has a Maker (e.g. `StPhiMaker`) that implements `Init()`, `Make()`, `Clear()`, `Finish()`. Histograms are created in `DeclareHistograms()` and written in `WriteHistograms()`. *Why:* keeps C++ physics logic in compiled code with a clear STAR lifecycle.

- **Makers are compiled.** Maker code lives in `StMaker/StXXXMaker/` and is built into `lib/libStXXXMaker.so`. This keeps heavy logic out of the interpreter and allows reuse.

- **Macros run under root4star.** The entry point is a ROOT macro invoked with `root4star -b -q "..."`. A small shell script (`script/run_anaXxx.sh`) sets the environment and calls the macro.

- **YAML-driven config.** Cuts and histogram definitions are read from YAML via `ConfigManager`, so you can change them without recompiling Makers when using existing cut/hist keys.

- **No hardcoding.** Do not hardcode concrete values in code; put all cut values, thresholds, and analysis parameters in YAML config so they can be changed without recompiling.

- **One config per concern.** Use one maker config per StMaker (referenced from mainconf). Use one cut config per cut type/source (e.g. one config for event cuts, one for track, one for PID, one for v0, one for mixing); mainconf references each. *Why:* smaller, reviewable YAML files and predictable composition.

- **Mainconf as the single entry point.** The main config (`config/mainconf/main_<anaName>.yaml`) references all other configs (paths relative to `config/`). Analysis should be fully reproducible using only the files referenced there. Scripts and workflows take mainconf as the primary argument so that setup, execution, I/O, and outputs are tied to one mainconf. *Why:* one identifier ties together `starver` choice (via analysis_info), local runs, batch joblists, and config snapshots (e.g. under `job/run/configlog/`).

## Configuration and reproducibility

The mainconf’s `analysis:` key points to an **analysis_info** YAML used by `script/setup.sh` (for `libraryTag` / `starver`) and by the joblist generator. Keeping **anaName**, paths, macro base names, and `starTag` fields consistent—often with YAML anchors—makes it easier to match outputs, logs, and submitted jobs to a single configuration set.

## Two macros per analysis (`run_anaXxx.C` and `anaXxx.C`)

ROOT's CINT interpreter does **not** resolve symbols from dynamically loaded shared libraries. So a single macro that does `gSystem->Load("libStPhiMaker.so")` and then `new StPhiMaker(...)` fails with "declared but not defined."

The fix is to **compile** the macro that uses the Maker with ACLiC (`.L anaPhi.C+`) and link it against the Maker library. That requires:

1. **Before** compilation: load STAR libs, load `libStXXXMaker.so`, and tell ACLiC to link against it (`gSystem->AddLinkedLibs(...)`).
2. **Then** compile and load the analysis macro (`.L anaXxx.C+`).
3. **Then** call the analysis function (e.g. `anaPhi(...)`).

Because `root4star -q` accepts only **one** macro invocation, that sequence is implemented in a **runner** macro, and the actual chain/event loop lives in a **second** macro that gets compiled:

- **run_anaXxx.C** — Loads STAR and project libs, sets `AddLinkedLibs`, compiles `anaXxx.C+`, and calls `anaXxx(...)`.
- **anaXxx.C** — Builds `StChain`, adds `StPicoDstMaker` and your Maker, runs the event loop. This file is compiled by ACLiC and linked to `libStXXXMaker.so`.

Each new analysis uses this two-file pattern on purpose; the runner is analysis-specific and not shared. *Common misconception:* merging runner and analysis into one macro is tempting but conflicts with ACLiC linking and the single `root4star -q` invocation pattern.

## Agents and human maintainers

When changing these principles, keep the AI source documents aligned:

- [docs/ai/AGENT_RULES.md](docs/ai/AGENT_RULES.md)
- [docs/ai/skills/](docs/ai/skills/)
- [CLAUDE.md](CLAUDE.md) (Antigravity entrypoint)

Cursor-facing files under [.cursor/rules/](.cursor/rules/) and [.cursor/skills/](.cursor/skills/) are wrappers and should remain concise pointers to source docs.

## Related documentation

- [README.md](README.md) — Overview and documentation index
- [INSTALL.md](INSTALL.md) — Clone, build, first local run
- [docs/REFERENCE.md](docs/REFERENCE.md) — Setup details, analysis_info, run/batch, adding analyses, config, joblists
