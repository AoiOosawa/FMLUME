#!/usr/bin/env python3
"""Factory preset audition helper (needs numpy + scipy and a built fmlume-render).

  python3 scripts/audition.py [--out build/audition] [--render build/tools/Release/fmlume-render]
                              [--only Pad,Bass] [--no-macros] [--no-page]

1. Screening: renders every factory preset with the same held notes and measures level, tone,
   attack / release, stereo and what each assigned macro does at 0 and 1. Presets that stand out
   are listed in <out>/report.md (all numbers in <out>/metrics.json).
2. Listening page: <out>/index.html plays one short clip per preset (same phrase within a
   category, levels untouched), shows the screening remarks beside it, lets you rate each sound
   with the keyboard and collects the ratings as text. Flagged macros get their own clips.

Numbers find what is broken or inconsistent. They do not tell whether a sound is musical.
"""
import argparse
import concurrent.futures as cf
import json
import os
import struct
import subprocess
import sys

import numpy as np
from scipy.io import wavfile

SR = 48000
HOLD, TAIL = 2.0, 4.0          # screening render: notes held 2 s, 4 s of tail
SLOT = 4.0                     # listening page: seconds per clip

# screening notes per category (register the sound is meant for)
ANALYSIS_NOTES = {"Bass": [36, 48], "Bell": [60, 72], "Lead": [60, 72]}
DEFAULT_NOTES = [48, 60]

# listening-page phrases: (start beat, length in beats, note), 120 bpm => 1 beat = 0.5 s, 6 beats = 3 s
CHORD = [(0, 6, 48), (0, 6, 55), (0, 6, 60), (0, 6, 64)]
PHRASES = {
    "Bass": [(0, .9, 40), (1, .4, 40), (1.5, .4, 52), (2, .9, 43), (3, .4, 45), (3.5, .4, 47), (4, 1.8, 40)],
    "Lead": [(0, .9, 69), (1, .4, 72), (1.5, .4, 74), (2, .9, 76), (3, .4, 74), (3.5, .4, 72), (4, 1.8, 69)],
    "Bell": [(0, 1, 72), (1, 1, 76), (2, 1, 79), (3, 1, 84), (4, 2, 72), (4, 2, 79)],
    "Pluck": [(0, .5, 57), (.5, .5, 60), (1, .5, 64), (1.5, .5, 69), (2, .5, 72), (2.5, .5, 69), (3, .5, 64), (3.5, .5, 60), (4, 2, 57), (4, 2, 64)],
    "E-Piano": [(0, 2, 48), (0, 2, 60), (0, 2, 64), (0, 2, 67), (2, .5, 72), (2.5, .5, 71), (3, 1, 67), (4, 2, 53), (4, 2, 60), (4, 2, 65), (4, 2, 69)],
    "Keys": [(0, 2, 48), (0, 2, 60), (0, 2, 64), (0, 2, 67), (2, .5, 72), (2.5, .5, 71), (3, 1, 67), (4, 2, 53), (4, 2, 60), (4, 2, 65), (4, 2, 69)],
    "Poly": [(0, 1.5, 48), (0, 1.5, 60), (0, 1.5, 64), (0, 1.5, 67), (2, 1.5, 45), (2, 1.5, 57), (2, 1.5, 60), (2, 1.5, 64), (4, 2, 53), (4, 2, 60), (4, 2, 65), (4, 2, 69)],
    "Brass": [(0, .4, 55), (0, .4, 60), (0, .4, 64), (1, .4, 55), (1, .4, 60), (1, .4, 64), (2, 4, 53), (2, 4, 60), (2, 4, 65)],
    "Pad": CHORD,
    "Arp": CHORD,
    "SFX": [(0, 6, 48)],
}


def write_midi(path, phrase, bpm=120):
    events = []
    for start, length, note in phrase:
        events.append((int(round(start * 480)), 1, bytes([0x90, note, 100])))
        events.append((int(round((start + length) * 480)), 0, bytes([0x80, note, 0])))
    events.sort(key=lambda e: (e[0], e[1]))

    def vlq(n):
        out = [n & 0x7F]
        while n > 0x7F:
            n >>= 7
            out.append((n & 0x7F) | 0x80)
        return bytes(reversed(out))

    track = b"\x00\xff\x51\x03" + int(60000000 / bpm).to_bytes(3, "big")
    last = 0
    for tick, _, data in events:
        track += vlq(tick - last) + data
        last = tick
    track += b"\x00\xff\x2f\x00"
    with open(path, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480) + b"MTrk" + struct.pack(">I", len(track)) + track)


def render(binary, preset, out, extra):
    r = subprocess.run([binary, "--preset", preset, "--out", out, "--sr", str(SR)] + extra, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"{os.path.basename(preset)}: {r.stdout.strip()} {r.stderr.strip()}")
    _, data = wavfile.read(out)
    os.remove(out)
    return data.astype(np.float64)


def db(x):
    return 20.0 * np.log10(np.maximum(x, 1e-9))


def a_weight(freq):
    f2 = np.maximum(freq, 1e-3) ** 2
    ra = (12194.0 ** 2 * f2 ** 2) / ((f2 + 20.6 ** 2) * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2)) * (f2 + 12194.0 ** 2))
    return ra * 1.2589  # 0 dB at 1 kHz


BAND_EDGES = 50.0 * 2.0 ** (np.arange(0, 27) / 3.0)  # third octaves 50 Hz .. 20 kHz


def band_frames(mono, frame=4800):
    """Third-octave band levels (dB) per 100 ms frame."""
    n = len(mono) // frame
    x = mono[: n * frame].reshape(n, frame) * np.hanning(frame)
    spec = np.abs(np.fft.rfft(x, axis=1)) ** 2
    freq = np.fft.rfftfreq(frame, 1.0 / SR)
    idx = np.searchsorted(freq, BAND_EDGES)
    bands = np.stack([spec[:, idx[i]:max(idx[i + 1], idx[i] + 1)].sum(axis=1) for i in range(len(BAND_EDGES) - 1)], axis=1)
    return 10.0 * np.log10(bands / frame + 1e-12)


def measure(data):
    L, R = data[:, 0], data[:, 1]
    mono = 0.5 * (L + R)
    m = {"finite": bool(np.all(np.isfinite(data)))}
    if not m["finite"]:
        return m, None
    m["peakDb"] = float(db(np.max(np.abs(data))))
    # loudness: A-weighted, loudest 400 ms
    spec = np.fft.rfft(mono)
    freq = np.fft.rfftfreq(len(mono), 1.0 / SR)
    aw = np.fft.irfft(spec * a_weight(freq), len(mono))
    win = int(0.4 * SR)
    cs = np.cumsum(np.concatenate([[0.0], aw ** 2]))
    m["loudDb"] = float(10.0 * np.log10(np.max(cs[win:] - cs[:-win]) / win + 1e-18))
    # tone over the held part
    held = mono[: int(HOLD * SR)]
    p = np.abs(np.fft.rfft(held * np.hanning(len(held)))) ** 2
    f = np.fft.rfftfreq(len(held), 1.0 / SR)
    total = p.sum() + 1e-18
    m["centroidHz"] = float((p * f).sum() / total)
    m["above6kPct"] = float(100.0 * p[f > 6000].sum() / total)
    m["below150Pct"] = float(100.0 * p[f < 150].sum() / total)
    # envelope (10 ms RMS)
    hop = SR // 100
    env = np.sqrt(np.mean((mono[: len(mono) // hop * hop].reshape(-1, hop)) ** 2, axis=1))
    top = env.max() + 1e-12
    m["attackS"] = float(np.argmax(env >= 0.5 * top) / 100.0)
    after = env[int(HOLD * 100):]
    quiet = np.nonzero(after > top * 10 ** (-50 / 20))[0]
    m["releaseS"] = float((quiet[-1] + 1) / 100.0) if len(quiet) else 0.0
    m["sustainDropDb"] = float(db(env[int(HOLD * 100) - 20:int(HOLD * 100)].mean()) - db(top))  # level just before note-off vs peak
    # stereo
    if np.std(L) > 1e-9 and np.std(R) > 1e-9:
        m["corr"] = float(np.corrcoef(L, R)[0, 1])
    else:
        m["corr"] = 1.0
    m["balanceDb"] = float(db(np.sqrt(np.mean(L ** 2))) - db(np.sqrt(np.mean(R ** 2))))
    m["dc"] = float(abs(np.mean(mono)))
    return m, band_frames(mono)


def distance(a, b):
    """Mean band-level difference in dB over the frames / bands where either render is audible."""
    floor = max(a.max(), b.max()) - 50.0
    mask = (a > floor) | (b > floor)
    if not mask.any():
        return 0.0
    return float(np.mean(np.abs(np.maximum(a, floor) - np.maximum(b, floor))[mask]))


def analyse(job):
    binary, path, tmp = job
    name = os.path.basename(path)[:-len(".fmlume")]
    with open(path) as fh:
        js = json.load(fh)
    cat = category_of(name)
    notes = ANALYSIS_NOTES.get(cat, DEFAULT_NOTES)
    base = sum([["--note", str(n)] for n in notes], []) + ["--dur", str(HOLD), "--tail", str(TAIL)]
    stem = os.path.join(tmp, name)
    m, frames = measure(render(binary, path, stem + ".wav", base))
    res = {"file": name, "name": js["displayName"], "category": cat, "metrics": m, "macros": [],
           "arp": bool(js["performance"]["arp"]["enabled"]), "gate": bool(js["performance"]["tranceGate"]["enabled"])}
    if frames is None or job_flags["no_macros"]:
        return res, frames
    for i, macro in enumerate(js["macros"]):
        if not macro.get("mappings"):
            continue
        entry = {"index": i + 1, "name": macro["name"], "default": macro["defaultValue"]}
        for label, value in (("lo", 0.0), ("hi", 1.0)):
            mm, fr = measure(render(binary, path, f"{stem}.m{i}{label}.wav", base + ["--macro", f"{i + 1}={value}"]))
            entry[label] = {"finite": mm["finite"]}
            if fr is not None:
                entry[label].update({"dist": distance(frames, fr), "loudDelta": mm["loudDb"] - m["loudDb"], "peakDb": mm["peakDb"]})
        res["macros"].append(entry)
    return res, frames


job_flags = {"no_macros": False}


def category_of(filename):
    return "E-Piano" if filename.startswith("E-Piano") else filename.split("-")[0]


def save_clip(path, data, seconds):
    n = int(seconds * SR)
    data = np.pad(data[:n], ((0, max(0, n - len(data))), (0, 0)))
    fade = int(0.25 * SR)
    data[-fade:] *= np.linspace(1.0, 0.0, fade)[:, None]
    wavfile.write(path, SR, np.round(np.clip(data, -1.0, 1.0) * 32767.0).astype(np.int16))


def make_clip(job):
    binary, preset, mid, out, extra, tmp = job
    data = render(binary, preset, os.path.join(tmp, os.path.basename(out)), ["--midi", mid, "--tail", "1.5"] + extra)
    save_clip(out, data, SLOT)


def listening_page(binary, presets_by_cat, out_dir, tmp, flags, macro_checks, redone=()):
    """One clip per preset (same phrase within a category, levels untouched) + index.html to rate them."""
    clips = os.path.join(out_dir, "clips")
    os.makedirs(clips, exist_ok=True)
    jobs, items = [], []
    for cat, paths in presets_by_cat.items():
        mid = os.path.join(tmp, cat + ".mid")
        write_midi(mid, PHRASES[cat])
        for n, path in enumerate(paths):
            name = os.path.basename(path)[:-len(".fmlume")]
            with open(path) as fh:
                js = json.load(fh)
            jobs.append((binary, path, mid, os.path.join(clips, name + ".wav"), [], tmp))
            macros = []
            for index, mname, values in macro_checks.get(name, []):
                for v in values:
                    clip = f"{name}.m{index}-{int(v)}.wav"
                    jobs.append((binary, path, mid, os.path.join(clips, clip), ["--macro", f"{index}={v}"], tmp))
                    macros.append({"id": f"m{index}-{int(v)}", "label": f"{mname} = {'最大' if v > 0.5 else '最小'}", "clip": clip})
            items.append({"file": name, "cat": cat, "num": n + 1, "name": js["displayName"], "desc": js.get("description", ""),
                          "tags": js.get("tags", []), "flags": [ja for _, ja in flags.get(name, [])], "macros": macros, "redone": name in redone})
    with cf.ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        list(ex.map(make_clip, jobs))
    for cat in presets_by_cat:
        os.remove(os.path.join(tmp, cat + ".mid"))
    page = PAGE.replace("/*DATA*/", json.dumps(items, ensure_ascii=False)).replace("/*STAMP*/", ",".join(sorted(redone)))
    with open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8") as fh:
        fh.write(page)


PAGE = r"""<!doctype html>
<html lang="ja"><head><meta charset="utf-8"><title>FMLUME 試聴</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;font:14px/1.5 -apple-system,"Hiragino Sans",sans-serif;background:#16181f;color:#e6e8ee;display:flex;height:100vh}
nav{width:190px;background:#1d2029;border-right:1px solid #2c303c;padding:14px 10px;overflow:auto;flex:none}
nav h1{font-size:15px;margin:0 6px 12px}
nav button{display:flex;justify-content:space-between;width:100%;padding:7px 10px;margin-bottom:2px;background:none;border:0;border-radius:6px;color:inherit;font:inherit;cursor:pointer;text-align:left}
nav button:hover{background:#262a36}
nav button.on{background:#2f6fed;color:#fff}
nav small{opacity:.7}
main{flex:1;display:flex;flex-direction:column;min-width:0}
header{padding:10px 18px;border-bottom:1px solid #2c303c;color:#9aa1b2;font-size:12.5px}
kbd{background:#2a2e3a;border-radius:4px;padding:1px 6px;color:#e6e8ee;font:12px ui-monospace,monospace}
#list{flex:1;overflow:auto;padding:8px 12px}
.row{display:grid;grid-template-columns:34px 1fr auto;gap:4px 12px;padding:9px 10px;border-radius:8px;border:1px solid transparent;cursor:pointer}
.row:hover{background:#1d2029}
.row.sel{background:#20242f;border-color:#2f6fed}
.row.playing .num{color:#4cc9f0}
.num{color:#727a8c;font-variant-numeric:tabular-nums;padding-top:1px}
.name{font-weight:600;font-size:15px}
.desc{color:#9aa1b2;font-size:12.5px}
.flags{margin-top:3px}
.redone{display:inline-block;background:#1c3350;color:#7fc1ff;border-radius:4px;padding:0 7px;margin:2px 4px 0 0;font-size:12px}
.flag{display:inline-block;background:#3a2f1c;color:#f0c674;border-radius:4px;padding:0 7px;margin:2px 4px 0 0;font-size:12px}
.rate{display:flex;gap:5px;align-items:flex-start;flex-wrap:wrap;justify-content:flex-end;max-width:420px}
.rate button,.macros button{background:#262a36;border:1px solid #363b4a;color:#c9cedb;border-radius:6px;padding:4px 10px;font:inherit;font-size:12.5px;cursor:pointer}
.rate button:hover,.macros button:hover{border-color:#5a6278}
.rate button.on[data-k=ok]{background:#1f6f43;border-color:#2fa866;color:#fff}
.rate button.on{background:#8a3b2c;border-color:#d0634a;color:#fff}
.extra{grid-column:2/4;display:none;gap:8px;flex-wrap:wrap;align-items:center}
.row.sel .extra,.row.has .extra{display:flex}
.macros{display:flex;gap:5px;flex-wrap:wrap;align-items:center;color:#9aa1b2;font-size:12.5px}
.macros button.bad{background:#8a3b2c;border-color:#d0634a;color:#fff}
input.note{flex:1;min-width:220px;background:#16181f;border:1px solid #363b4a;border-radius:6px;color:inherit;font:inherit;padding:4px 8px}
footer{border-top:1px solid #2c303c;padding:10px 18px;display:flex;gap:12px;align-items:center}
footer button{background:#2f6fed;border:0;color:#fff;border-radius:6px;padding:7px 14px;font:inherit;cursor:pointer}
footer button.ghost{background:#262a36;color:#c9cedb}
#result{display:none;position:fixed;inset:8% 12%;background:#1d2029;border:1px solid #363b4a;border-radius:10px;padding:16px;flex-direction:column;gap:10px;box-shadow:0 10px 60px #000a}
#result textarea{flex:1;background:#16181f;color:inherit;border:1px solid #363b4a;border-radius:6px;font:12.5px ui-monospace,monospace;padding:10px}
</style></head><body>
<nav><h1>FMLUME 試聴</h1><div id="cats"></div></nav>
<main>
<header><kbd>↑</kbd><kbd>↓</kbd> 前後の音色を再生　<kbd>Space</kbd> もう一度　<kbd>1</kbd> OK　<kbd>2</kbd> 大きい　<kbd>3</kbd> 小さい　<kbd>4</kbd> 音がいまいち　<kbd>5</kbd> 他と似すぎ　<kbd>←</kbd><kbd>→</kbd> カテゴリ切替　／　同じカテゴリは同じフレーズ・音量は無加工。黄色は数値チェックの指摘。</header>
<div id="list"></div>
<footer><span id="progress"></span><span id="redoneNote" style="color:#7fc1ff"></span><span style="flex:1"></span><button class="ghost" id="reset">評価を全部消す</button><button id="export">結果をまとめる</button></footer>
</main>
<div id="result"><b>この内容をコピーして Claude に貼り付けてください</b><textarea readonly></textarea><div style="display:flex;gap:10px;justify-content:flex-end"><button id="copy">コピー</button><button id="close">閉じる</button></div></div>
<script>
const DATA = /*DATA*/;
const KEYS = [["ok","OK"],["loud","大きい"],["quiet","小さい"],["tone","音がいまいち"],["dup","他と似すぎ"]];
const cats = [...new Set(DATA.map(d => d.cat))];
let state = JSON.parse(localStorage.getItem("fmlume-audition") || "{}");
if (state._stamp !== "/*STAMP*/") { for (const d of DATA) if (d.redone) delete state[d.file]; state._stamp = "/*STAMP*/"; localStorage.setItem("fmlume-audition", JSON.stringify(state)); }
let cat = cats[0], sel = 0, audio = new Audio(), playingRow = null;
const save = () => localStorage.setItem("fmlume-audition", JSON.stringify(state));
const st = f => state[f] || (state[f] = {tags: [], macros: [], note: ""});
const rated = d => { const s = state[d.file]; return s && (s.tags.length || s.macros.length || s.note); };
const items = () => DATA.filter(d => d.cat === cat);
function play(clip, row) {
  audio.pause(); audio = new Audio("clips/" + encodeURIComponent(clip)); audio.play();
  if (playingRow) playingRow.classList.remove("playing");
  playingRow = row; if (row) row.classList.add("playing");
}
function renderCats() {
  const el = document.getElementById("cats"); el.innerHTML = "";
  for (const c of cats) {
    const all = DATA.filter(d => d.cat === c), done = all.filter(rated).length;
    const b = document.createElement("button"); b.className = c === cat ? "on" : "";
    const re = all.filter(d => d.redone).length;
    b.innerHTML = `<span>${c}${re ? ` <small style="color:#7fc1ff">●${re}</small>` : ""}</span><small>${done}/${all.length}</small>`;
    b.onclick = () => { cat = c; sel = 0; renderAll(); };
    el.appendChild(b);
  }
  const done = DATA.filter(rated).length;
  document.getElementById("redoneNote").textContent = DATA.some(d => d.redone) ? `青い印 = 作り直した音色（${DATA.filter(d => d.redone).length}）` : "";
  document.getElementById("progress").textContent = `評価済み ${done} / ${DATA.length}（評価は自動保存。付けなかった音色は「指摘なし」として扱います）`;
}
function renderList() {
  const el = document.getElementById("list"); el.innerHTML = "";
  items().forEach((d, i) => {
    const s = st(d.file), row = document.createElement("div");
    row.className = "row" + (i === sel ? " sel" : "") + ((d.macros.length && d.flags.length) || s.note ? " has" : "");
    row.innerHTML = `<div class="num">${String(d.num).padStart(2, "0")}</div>
      <div><div class="name">${d.name}</div><div class="desc">${d.desc}</div>
      <div class="flags">${d.redone ? `<span class="redone">作り直しました</span>` : ""}${d.flags.map(f => `<span class="flag">${f}</span>`).join("")}</div></div>
      <div class="rate">${KEYS.map(([k, label], n) => `<button data-k="${k}" class="${s.tags.includes(k) ? "on" : ""}" title="キー ${n + 1}">${label}</button>`).join("")}</div>
      <div class="extra">${d.macros.length ? `<div class="macros">マクロを動かした音:${d.macros.map(m => `<button data-clip="${m.clip}">▶ ${m.label}</button><button data-m="${m.id}" class="${s.macros.includes(m.id) ? "bad" : ""}">気になる</button>`).join("")}</div>` : ""}
      <input class="note" placeholder="メモ（任意）" value="${(s.note || "").replace(/"/g, "&quot;")}"></div>`;
    row.onclick = e => {
      const t = e.target;
      if (t.dataset.k) { toggle(d.file, t.dataset.k); return; }
      if (t.dataset.clip) { sel = i; play(t.dataset.clip, null); markSel(); return; }
      if (t.dataset.m) { const a = st(d.file).macros, x = a.indexOf(t.dataset.m); x < 0 ? a.push(t.dataset.m) : a.splice(x, 1); save(); t.classList.toggle("bad"); renderCats(); return; }
      if (t.classList.contains("note")) { sel = i; markSel(); return; }
      select(i);
    };
    row.querySelector(".note").oninput = e => { st(d.file).note = e.target.value; save(); renderCats(); };
    el.appendChild(row);
  });
}
function markSel() { [...document.getElementById("list").children].forEach((r, i) => r.classList.toggle("sel", i === sel)); }
function select(i) {
  const list = items(); sel = Math.max(0, Math.min(list.length - 1, i)); markSel();
  const row = document.getElementById("list").children[sel];
  row.scrollIntoView({block: "nearest"}); play(list[sel].file + ".wav", row);
}
function toggle(file, k) {
  const a = st(file).tags, x = a.indexOf(k);
  if (x >= 0) a.splice(x, 1); else { if (k === "ok") a.length = 0; else if (a.includes("ok")) a.splice(a.indexOf("ok"), 1); if (k === "loud" && a.includes("quiet")) a.splice(a.indexOf("quiet"), 1); if (k === "quiet" && a.includes("loud")) a.splice(a.indexOf("loud"), 1); a.push(k); }
  save();
  const row = [...document.getElementById("list").children][items().findIndex(d => d.file === file)];
  row.querySelectorAll(".rate button").forEach(b => b.classList.toggle("on", a.includes(b.dataset.k)));
  renderCats();
}
function renderAll() { renderCats(); renderList(); }
document.addEventListener("keydown", e => {
  if (e.target.tagName === "INPUT" || e.target.tagName === "TEXTAREA") { if (e.key === "Escape" || e.key === "Enter") e.target.blur(); return; }
  const list = items();
  if (e.key === "ArrowDown") { select(sel + 1); e.preventDefault(); }
  else if (e.key === "ArrowUp") { select(sel - 1); e.preventDefault(); }
  else if (e.key === " ") { select(sel); e.preventDefault(); }
  else if (e.key === "ArrowRight" || e.key === "ArrowLeft") { cat = cats[(cats.indexOf(cat) + (e.key === "ArrowRight" ? 1 : cats.length - 1)) % cats.length]; sel = 0; renderAll(); e.preventDefault(); }
  else if (e.key >= "1" && e.key <= "5") toggle(list[sel].file, KEYS[+e.key - 1][0]);
});
document.getElementById("export").onclick = () => {
  const label = Object.fromEntries(KEYS), lines = [];
  for (const d of DATA) {
    const s = state[d.file]; if (!s || !(s.tags.length || s.macros.length || s.note)) continue;
    const parts = s.tags.map(t => label[t]).concat(s.macros.map(id => "マクロ気になる: " + (d.macros.find(m => m.id === id) || {label: id}).label));
    if (s.note) parts.push("メモ: " + s.note);
    lines.push(`${d.file}: ${parts.join(" / ")}`);
  }
  const untouched = DATA.filter(d => !rated(d)).length;
  const box = document.getElementById("result");
  box.querySelector("textarea").value = `FMLUME 試聴結果（評価 ${DATA.length - untouched} / ${DATA.length}、未評価 ${untouched}）\n` + lines.join("\n");
  box.style.display = "flex"; box.querySelector("textarea").select();
};
document.getElementById("copy").onclick = () => { const t = document.querySelector("#result textarea"); t.select(); document.execCommand("copy"); };
document.getElementById("close").onclick = () => document.getElementById("result").style.display = "none";
document.getElementById("reset").onclick = () => { if (document.getElementById("reset").dataset.armed) { state = {}; save(); renderAll(); delete document.getElementById("reset").dataset.armed; document.getElementById("reset").textContent = "評価を全部消す"; } else { document.getElementById("reset").dataset.armed = 1; document.getElementById("reset").textContent = "もう一度押すと消えます"; } };
renderAll();
</script></body></html>
"""


def find_flags(results, frames):
    """Returns {file: [(english, japanese)]}, the near-duplicate pairs and the macro settings worth hearing."""
    flags = {r["file"]: [] for r in results}
    macro_checks = {}
    ok = [r for r in results if r["metrics"].get("finite")]
    med_all = float(np.median([r["metrics"]["loudDb"] for r in ok]))
    by_cat = {}
    for r in ok:
        by_cat.setdefault(r["category"], []).append(r)
    for r in results:
        m, f, cat = r["metrics"], flags[r["file"]], r["category"]
        add = lambda en, ja: f.append((en, ja))
        if not m.get("finite"):
            add("non-finite output", "出力が異常値")
            continue
        med = float(np.median([x["metrics"]["loudDb"] for x in by_cat[cat]]))
        if m["peakDb"] < -40:
            add(f"almost silent (peak {m['peakDb']:.1f} dBFS)", "ほぼ無音")
        if m["peakDb"] > -0.3:
            add(f"at the limiter (peak {m['peakDb']:.1f} dBFS)", "リミッターに当たっている")
        if abs(m["loudDb"] - med) > 4.0:
            d = m["loudDb"] - med
            add(f"loudness {d:+.1f} dB vs the {cat} median", f"{cat} の中で{'大きい' if d > 0 else '小さい'}（{d:+.1f} dB）")
        elif abs(m["loudDb"] - med_all) > 7.0:
            d = m["loudDb"] - med_all
            add(f"loudness {d:+.1f} dB vs all presets", f"全体の中で{'大きい' if d > 0 else '小さい'}（{d:+.1f} dB）")
        if m["above6kPct"] > 25 and cat != "SFX":
            add(f"harsh: {m['above6kPct']:.0f}% of the energy above 6 kHz", "高域がきつい可能性")
        cmed = float(np.median([x["metrics"]["centroidHz"] for x in by_cat[cat]]))
        if cat != "SFX" and m["centroidHz"] < 0.4 * cmed:
            add(f"much duller than the other {cat} sounds (centroid {m['centroidHz']:.0f} Hz, median {cmed:.0f} Hz)", f"他の {cat} よりかなり暗い")
        if cat != "SFX" and m["centroidHz"] > 3.0 * cmed:
            add(f"much brighter than the other {cat} sounds (centroid {m['centroidHz']:.0f} Hz, median {cmed:.0f} Hz)", f"他の {cat} よりかなり明るい")
        if m["attackS"] > 0.35 and cat not in ("Pad", "SFX"):
            add(f"slow attack ({m['attackS']:.2f} s) for {cat}", f"立ち上がりが遅い（{m['attackS']:.2f} 秒）")
        if m["releaseS"] >= TAIL - 0.05 and cat not in ("Pad", "SFX"):
            add(f"still sounding {TAIL:.0f} s after note-off", "余韻が非常に長い")
        if m["releaseS"] < 0.03 and m["sustainDropDb"] > -30.0 and cat not in ("Bass", "Lead", "SFX", "Arp"):
            add("release cut dead (< 30 ms)", "音がぶつ切れ")
        if m["corr"] < 0.0:
            add(f"left / right out of phase (correlation {m['corr']:.2f}): thin in mono", "左右が逆相気味（モノラルで痩せる）")
        if abs(m["balanceDb"]) > 4.0:
            add(f"off-centre ({m['balanceDb']:+.1f} dB left vs right)", f"{'左' if m['balanceDb'] > 0 else '右'}に寄っている（{abs(m['balanceDb']):.1f} dB）")
        if m["dc"] > 0.01:
            add(f"DC offset {m['dc']:.3f}", "DC オフセット")
        for mc in r["macros"]:
            tag, tj = f"macro {mc['index']} \"{mc['name']}\"", f"マクロ{mc['index']} {mc['name']}"
            lo, hi = mc["lo"], mc["hi"]
            hear = set()
            if not (lo.get("finite") and hi.get("finite")):
                add(f"{tag}: non-finite output", f"{tj}: 出力が異常値")
                continue
            # the range actually reachable from the stored value
            reach = [(x, v) for x, v in ((lo, 0.0), (hi, 1.0)) if abs(mc["default"] - v) > 0.05]
            if "attack" not in mc["name"].lower() and reach and max(max(x["dist"], abs(x["loudDelta"])) for x, _ in reach) < 0.7:
                add(f"{tag}: barely audible (max change {max(x['dist'] for x, _ in reach):.1f} dB)", f"{tj}: 効きがほぼ聞こえない")
                hear.update(v for _, v in reach)
            for x, end in ((lo, 0.0), (hi, 1.0)):
                ej = "最大" if end > 0.5 else "最小"
                if x["loudDelta"] > 8.0:
                    add(f"{tag} at {end:.0f}: {x['loudDelta']:+.1f} dB louder", f"{tj} {ej}: 音量が {x['loudDelta']:+.1f} dB 跳ねる")
                    hear.add(end)
                if x["loudDelta"] < -18.0 and "attack" not in mc["name"].lower():
                    add(f"{tag} at {end:.0f}: nearly silences the sound ({x['loudDelta']:+.1f} dB)", f"{tj} {ej}: ほぼ無音になる")
                    hear.add(end)
                if x["peakDb"] > -0.3 and m["peakDb"] <= -0.3:
                    add(f"{tag} at {end:.0f}: drives the output into the limiter", f"{tj} {ej}: リミッターに当たる")
                    hear.add(end)
            if hear:
                macro_checks.setdefault(r["file"], []).append((mc["index"], mc["name"], sorted(hear)))
    # near duplicates (same screening notes only)
    pairs = []
    names = [r["file"] for r in ok]
    for i in range(len(ok)):
        for j in range(i + 1, len(ok)):
            a, b = ok[i], ok[j]
            if ANALYSIS_NOTES.get(a["category"], DEFAULT_NOTES) != ANALYSIS_NOTES.get(b["category"], DEFAULT_NOTES):
                continue
            fa, fb = frames[names[i]], frames[names[j]]
            d = distance(fa - fa.max(), fb - fb.max())
            if d < 2.0:
                pairs.append((d, a["file"], b["file"]))
                flags[a["file"]].append((f"nearly the same as {b['file']}", f"{b['name']} とほぼ同じ音"))
                flags[b["file"]].append((f"nearly the same as {a['file']}", f"{a['name']} とほぼ同じ音"))
    return flags, sorted(pairs), macro_checks


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", default="build/tools/Release/fmlume-render")
    ap.add_argument("--presets", default="Resources/Presets")
    ap.add_argument("--out", default="build/audition")
    ap.add_argument("--only", default="")
    ap.add_argument("--no-macros", action="store_true")
    ap.add_argument("--no-page", action="store_true")
    ap.add_argument("--redone", default="", help="comma-separated preset file names (no extension) to mark as reworked; their old ratings are dropped")
    ap.add_argument("--no-screen", action="store_true")
    args = ap.parse_args()
    job_flags["no_macros"] = args.no_macros
    if not os.path.exists(args.render):
        sys.exit(f"{args.render} not found: build the project first")
    only = [c for c in args.only.split(",") if c]
    by_cat = {}
    for fn in sorted(os.listdir(args.presets)):
        if not fn.endswith(".fmlume"):
            continue
        cat = category_of(fn)
        if only and cat not in only:
            continue
        by_cat.setdefault(cat, []).append(os.path.join(args.presets, fn))
    tmp = os.path.join(args.out, "tmp")
    os.makedirs(tmp, exist_ok=True)

    flags, macro_checks = {}, {}
    if not args.no_screen:
        jobs = [(args.render, p, tmp) for paths in by_cat.values() for p in paths]
        results, frames = [], {}
        with cf.ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
            for res, fr in ex.map(analyse, jobs):
                results.append(res)
                if fr is not None:
                    frames[res["file"]] = fr
        flags, pairs, macro_checks = find_flags(results, frames)
        with open(os.path.join(args.out, "metrics.json"), "w") as fh:
            json.dump(results, fh, indent=1)
        lines = ["# FMLUME factory preset screening", "",
                 f"{len(results)} presets, {sum(1 for v in flags.values() if v)} flagged, {len(pairs)} near-duplicate pair(s).",
                 "Screening notes held 2 s at velocity 100; macros compared at 0 and 1 against the stored value.", "", "## Flagged", ""]
        for r in results:
            if flags[r["file"]]:
                lines.append(f"- **{r['file']}**")
                lines += [f"  - {en}" for en, _ in flags[r["file"]]]
        lines += ["", "## Near duplicates (level-normalised spectrum over time, < 2 dB apart)", ""]
        lines += [f"- {a}  ~  {b}  ({d:.2f} dB)" for d, a, b in pairs] or ["- none"]
        lines += ["", "## All presets", "", "| preset | peak dBFS | loud dBA | centroid Hz | >6k % | attack s | release s | L/R corr |", "|---|---|---|---|---|---|---|---|"]
        for r in results:
            m = r["metrics"]
            if m.get("finite"):
                lines.append(f"| {r['file']} | {m['peakDb']:.1f} | {m['loudDb']:.1f} | {m['centroidHz']:.0f} | {m['above6kPct']:.1f} | {m['attackS']:.2f} | {m['releaseS']:.2f} | {m['corr']:.2f} |")
        with open(os.path.join(args.out, "report.md"), "w") as fh:
            fh.write("\n".join(lines) + "\n")
        print("\n".join(lines[:lines.index("## All presets")]))

    if not args.no_page:
        listening_page(args.render, by_cat, args.out, tmp, flags, macro_checks, set(c for c in args.redone.split(",") if c))
        print(f"listening page: {os.path.join(args.out, 'index.html')}")
    try:
        os.rmdir(tmp)
    except OSError:
        pass


if __name__ == "__main__":
    main()
