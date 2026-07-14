#!/usr/bin/env python3
"""Generate config/hist/hist_anaFemtoKaon.yaml for unified K- femtoscopy."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HIST_DIR = ROOT / "config" / "hist"

PHI_FEMTO_KEYS = {
    "hPhi_MKK_allForFemto",
    "hPhi_MKK_signal",
    "hPhi_MKK_leftSB",
    "hPhi_MKK_rightSB",
    "hPhi_MKK_rot",
    "hPhi_PtVsY_PostCut",
    "hPhi_PtVsY_signal",
    "hPhi_PtVsY_rot",
    "hPhiRot_MKK",
    "hPhiRot_Pt",
    "hPhiRot_Rapidity",
    "hPhiRot_NCand",
    "hPhiRot_DeltaPhiApplied",
}
SKIP_PREFIXES = ("hPhi", "hPhiRot", "hOpeningAngle", "hPair", "hMKK", "hDCA_KK")
CHANNELS = [
    "kaon_minus_proton",
    "kaon_minus_deuteron",
    "kaon_minus_triton",
    "kaon_minus_he3",
    "kaon_minus_he4",
]


def parse_axes(text: str) -> str:
    m = re.search(r"^axes:\s*\n", text, re.MULTILINE)
    if not m:
        return ""
    start = m.end()
    m2 = re.search(r"^histograms:\s*$", text[start:], re.MULTILINE)
    end = start + m2.start() if m2 else len(text)
    return text[start:end]


def merge_axes(*sections: str) -> str:
    blocks: dict[str, list[str]] = {}
    order: list[str] = []
    for section in sections:
        current: list[str] = []
        current_name = ""
        for line in section.splitlines():
            if line.strip() == "axes:":
                continue
            if line.strip().startswith("#"):
                continue
            m = re.match(r"^  ([A-Za-z0-9_]+):", line)
            if m:
                name = m.group(1)
                if name.startswith("h"):
                    continue
                if current_name and current_name not in blocks:
                    blocks[current_name] = current
                    order.append(current_name)
                current_name = name
                current = [line]
            elif current_name:
                current.append(line)
        if current_name and current_name not in blocks:
            blocks[current_name] = current
            order.append(current_name)
    lines = ["axes:"]
    for name in order:
        lines.extend(blocks[name])
    return "\n".join(lines) + "\n\n"


def parse_hist_blocks(text: str) -> dict[str, str]:
    m = re.search(r"^histograms:\s*$", text, re.MULTILINE)
    if not m:
        raise RuntimeError("histograms: section not found")
    body = text[m.end() :]
    blocks: dict[str, str] = {}
    for match in re.finditer(r"^  (h[A-Za-z0-9_]+):\s*\n", body, re.MULTILINE):
        name = match.group(1)
        start = match.start()
        nxt = re.search(r"^  h[A-Za-z0-9_]+:\s*\n", body[match.end() :], re.MULTILINE)
        end = match.end() + nxt.start() if nxt else len(body)
        blocks[name] = body[start:end].rstrip() + "\n"
    return blocks


def p_to_km(block: str) -> str:
    out = block.replace("hP_", "hKm_")
    out = out.replace("n#sigma_{p}", "n#sigma_{K}")
    out = out.replace("N_{p}", "N_{K^{-}}")
    out = out.replace("Proton", "K^{-}")
    out = out.replace("proton", "K^{-}")
    out = out.replace("NSigmaProton", "NSigmaKaon")
    return out


def channel_from_template(tmpl: str, channel: str) -> str:
    return re.sub(r"phi_proton", channel, tmpl)


def main() -> None:
    phi_unified = (HIST_DIR / "hist_anaFemtoPhi.yaml").read_text()
    proton_text = (HIST_DIR / "hist_anaFemtoPhiProton.yaml").read_text()
    deuteron_text = (HIST_DIR / "hist_anaFemtoPhiDeuteron.yaml").read_text()
    he4_text = (HIST_DIR / "hist_anaFemtoPhi4He.yaml").read_text()

    phi_blocks = parse_hist_blocks(phi_unified)
    proton_blocks = parse_hist_blocks(proton_text)
    deuteron_blocks = parse_hist_blocks(deuteron_text)
    he4_blocks = parse_hist_blocks(he4_text)

    merged_axes = merge_axes(
        parse_axes(phi_unified),
        parse_axes(proton_text),
        parse_axes(deuteron_text),
        parse_axes(he4_text),
    )
    # anaFemtoKaon: k* QA and CF use 0–2.0 GeV/c (checkHist kKstarHistXMax).
    merged_axes = re.sub(
        r"  Kstar: &Kstar\n    nBins: \d+\n    min: 0\.0\n    max: [\d.]+",
        "  Kstar: &Kstar\n    nBins: 200\n    min: 0.0\n    max: 2.0",
        merged_axes,
        count=1,
    )

    common: dict[str, str] = {}
    for name, block in phi_blocks.items():
        if name.startswith("hKstar") or name.startswith("hCF_"):
            continue
        if name in PHI_FEMTO_KEYS:
            continue
        if any(name.startswith(p) for p in SKIP_PREFIXES):
            continue
        if name.startswith(("hP_", "hDeuteron_", "hTriton_", "hHe3_", "hHe4_")):
            continue
        if name.startswith("hN") and name.endswith("_vs_Cent9"):
            continue
        common[name] = block

    common["hNKaonMinusFemto"] = """  hNKaonMinusFemto:
    axis: *NKaon
    title: "N_{K^{-}} femto candidates per event;N_{K^{-}};Counts"
"""

    kaon: dict[str, str] = {}
    for name, block in proton_blocks.items():
        if name.startswith("hP_"):
            km_name = name.replace("hP_", "hKm_")
            kaon[km_name] = p_to_km(block)
    kaon["hNKaonMinus_vs_Cent9"] = """  hNKaonMinus_vs_Cent9:
    xAxis: *Cent9
    yAxis: *NKaon
    title: "N_{K^{-}} vs cent9;cent9;N_{K^{-}}"
"""

    bachelor: dict[str, str] = {}
    bachelor.update({k: v for k, v in phi_blocks.items() if k.startswith("hP_")})
    bachelor["hNProton_vs_Cent9"] = phi_blocks.get("hNProton_vs_Cent9", "")
    for pref in ("hDeuteron_", "hTriton_", "hHe3_", "hHe4_"):
        bachelor.update({k: v for k, v in phi_blocks.items() if k.startswith(pref)})
    for nc in ("hNDeuteron_vs_Cent9", "hNTriton_vs_Cent9", "hNHe3_vs_Cent9", "hNHe4_vs_Cent9"):
        if nc in phi_blocks:
            bachelor[nc] = phi_blocks[nc]

    channel_blocks: dict[str, str] = {}
    kstar_kinds = ("hKstarSE_", "hKstarME_", "hCF_", "hKstarSEVsCent_", "hKstarMEVsCent_")
    proton_tpl = {k: proton_blocks[k] for k in proton_blocks if any(k.startswith(p) for p in kstar_kinds)}
    for ch in CHANNELS:
        for hk, block in proton_tpl.items():
            if "_phi_proton" not in hk:
                continue
            new_name = hk.replace("phi_proton", ch)
            new_block = channel_from_template(block, ch)
            new_block = new_block.replace("phi_deuteron", ch).replace("phi_he4", ch).replace("phi_triton", ch)
            new_block = new_block.replace("phi_he3", ch).replace("phi_proton", ch)
            channel_blocks[new_name] = new_block
        for kind in ("hKstarSEVsCent_", "hKstarMEVsCent_"):
            nm = kind + ch
            if nm in channel_blocks:
                continue
            src = kind + "phi_he4_signal"
            if src in he4_blocks:
                block = channel_from_template(he4_blocks[src], ch)
                block = block.replace("phi_he4_signal", ch).replace("signal", ch)
                channel_blocks[nm] = block

    out_header = "# StFemtoMaker histogram definitions (auau3p85fxt_anaFemtoKaon unified)\n"
    out_header += "# 1D: axis: *Preset or nBins/min/max; title required\n"
    out_header += "# 2D: xAxis: *Preset, yAxis: *Preset; title required\n\n"
    out_header += merged_axes

    ordered: list[str] = []
    ordered.extend(sorted(common.keys()))
    ordered.extend(sorted(kaon.keys()))
    ordered.extend(sorted(k for k in bachelor if k.startswith("hP_")))
    ordered.append("hNProton_vs_Cent9")
    for pref in ("hDeuteron_", "hTriton_", "hHe3_", "hHe4_"):
        ordered.extend(sorted(k for k in bachelor if k.startswith(pref)))
    for nc in ("hNDeuteron_vs_Cent9", "hNTriton_vs_Cent9", "hNHe3_vs_Cent9", "hNKaonMinus_vs_Cent9"):
        if nc in bachelor or nc in kaon:
            ordered.append(nc)
    for ch in CHANNELS:
        for kind in ("hKstarSE_", "hKstarME_", "hCF_", "hKstarSEVsCent_", "hKstarMEVsCent_"):
            nm = kind + ch
            if nm in channel_blocks:
                ordered.append(nm)

    merged: dict[str, str] = {}
    merged.update(common)
    merged.update(kaon)
    merged.update(bachelor)
    merged.update(channel_blocks)

    lines = [out_header.rstrip(), "", "histograms:"]
    seen: set[str] = set()
    for name in ordered:
        if name in merged and name not in seen:
            lines.append(merged[name].rstrip())
            lines.append("")
            seen.add(name)

    out_path = HIST_DIR / "hist_anaFemtoKaon.yaml"
    out_path.write_text("\n".join(lines).rstrip() + "\n")
    print(f"Wrote {out_path} ({len(seen)} histograms)")


if __name__ == "__main__":
    main()
