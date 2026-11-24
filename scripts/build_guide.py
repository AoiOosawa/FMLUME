#!/usr/bin/env python3
"""docs/USER_GUIDE.md から公開用の site/guide.html を作る。

    python3 scripts/build_guide.py          # site/guide.html を書き出す
    python3 scripts/build_guide.py --check  # 最新でなければ終了コード 1（CI 用）

元の文書は Markdown のまま（リポジトリで読める）にして、HTML はここから作る。
外部のライブラリは使わない。ガイドで使っている書き方（見出し、段落、箇条書き、
番号付きリスト、表、引用、太字、コード、リンク）だけを扱う。
"""
import html
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "docs" / "USER_GUIDE.md"
OUT = ROOT / "site" / "guide.html"
REPO = "https://github.com/cheebow/FMLUME"


def inline(text: str) -> str:
    """太字・コード・リンクを HTML にする（それ以外の文字はエスケープ）。"""
    parts = re.split(r"(`[^`]+`)", text)
    out = []
    for part in parts:
        if part.startswith("`") and part.endswith("`") and len(part) > 1:
            out.append("<code>" + html.escape(part[1:-1]) + "</code>")
            continue
        s = html.escape(part, quote=False)
        s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)

        def link(m):
            label, url = m.group(1), m.group(2)
            if not re.match(r"^(https?:|#)", url):
                # リポジトリ内のファイルへのリンクは GitHub 上のファイルを指す
                url = REPO + "/blob/main/" + re.sub(r"^(\.\./)+", "", "docs/" + url if not url.startswith("../") else url)
            return '<a href="%s">%s</a>' % (html.escape(url), label)

        s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", link, s)
        out.append(s)
    return "".join(out)


def join(lines) -> str:
    """改行で分かれた文をつなぐ。日本語どうしの間には空白を入れない。"""
    out = ""
    for x in lines:
        if out and not (ord(out[-1]) > 0x2E7F and ord(x[0]) > 0x2E7F):
            out += " "
        out += x
    return out


def slug(n: int) -> str:
    return "s%d" % n


def convert(md: str):
    """本文の HTML、目次の項目、タイトル、導入文を返す。"""
    lines = md.splitlines()
    body, toc = [], []
    title, intro = "", []
    i, h2count, h3count = 0, 0, 0
    para = []

    def flush_para():
        if para:
            body.append("<p>" + inline(join(para)) + "</p>")
            para.clear()

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if not stripped:
            flush_para()
            i += 1
            continue
        if line.startswith("# "):
            title = line[2:].strip()
            i += 1
            continue
        if line.startswith("## ") or line.startswith("### "):
            flush_para()
            level = 2 if line.startswith("## ") else 3
            text = line[level + 1:].strip()
            if level == 2:
                h2count += 1
                h3count = 0
                anchor = slug(h2count)
                toc.append((text, anchor, []))
            else:
                h3count += 1
                anchor = "%s-%d" % (slug(h2count), h3count)
                if toc:
                    toc[-1][2].append((text, anchor))
            body.append('<h%d id="%s">%s</h%d>' % (level, anchor, inline(text), level))
            i += 1
            continue
        if stripped.startswith(">"):
            flush_para()
            quote = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                quote.append(lines[i].strip()[1:].strip())
                i += 1
            body.append('<div class="note">' + inline(join(quote)) + "</div>")
            continue
        if stripped.startswith("|"):
            flush_para()
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            head, rest = rows[0], [r for r in rows[1:] if not all(re.fullmatch(r":?-{2,}:?", c) for c in r)]
            t = ['<div class="table-scroll"><table><thead><tr>']
            t += ["<th>%s</th>" % inline(c) for c in head]
            t.append("</tr></thead><tbody>")
            for r in rest:
                t.append("<tr>" + "".join("<td>%s</td>" % inline(c) for c in r) + "</tr>")
            t.append("</tbody></table></div>")
            body.append("".join(t))
            continue
        m_ul = re.match(r"^- (.*)", line)
        m_ol = re.match(r"^\d+\. (.*)", line)
        if m_ul or m_ol:
            flush_para()
            tag = "ul" if m_ul else "ol"
            pattern = r"^- (.*)" if m_ul else r"^\d+\. (.*)"
            items = []
            while i < len(lines):
                m = re.match(pattern, lines[i])
                if m:
                    items.append(m.group(1))
                elif lines[i].startswith("  ") and lines[i].strip() and items and not lines[i].strip().startswith("|"):
                    items[-1] = join([items[-1], lines[i].strip()])  # 前の項目の続き
                else:
                    break
                i += 1
            body.append("<%s>%s</%s>" % (tag, "".join("<li>%s</li>" % inline(x) for x in items), tag))
            continue
        if not toc and title and not body:
            intro.append(stripped)
            i += 1
            continue
        para.append(stripped)
        i += 1
    flush_para()
    return "\n".join(body), toc, title, intro


STYLE = """
  :root {
    --bg: #0d0e17; --bg-raised: #171926; --bg-card: #1c1f30; --line: rgba(255, 255, 255, 0.09);
    --text: #f4f5fb; --muted: #a3a7bd; --cyan: #3cd6ff; --violet: #8a6bff; --yellow: #ffd23c;
    --grad: linear-gradient(90deg, var(--cyan), var(--violet) 55%, #d74db5);
    --max: 1180px;
  }
  * { box-sizing: border-box; }
  html { scroll-behavior: smooth; scroll-padding-top: 76px; }
  body {
    margin: 0; background: var(--bg); color: var(--text); line-height: 1.8;
    font-family: "Avenir Next", "Hiragino Sans", "Hiragino Kaku Gothic ProN", "Noto Sans JP", "Helvetica Neue", Arial, sans-serif;
    -webkit-font-smoothing: antialiased;
  }
  a { color: var(--cyan); }
  .wrap { max-width: var(--max); margin: 0 auto; padding: 0 24px; }
  header.nav {
    position: sticky; top: 0; z-index: 10; background: rgba(13, 14, 23, 0.86);
    backdrop-filter: blur(14px); -webkit-backdrop-filter: blur(14px); border-bottom: 1px solid var(--line);
  }
  .nav .wrap { display: flex; align-items: center; justify-content: space-between; height: 60px; }
  .brand { display: flex; align-items: center; gap: 10px; text-decoration: none; color: var(--text); font-weight: 800; letter-spacing: 0.12em; }
  .brand img { width: 38px; height: auto; }
  .nav nav { display: flex; gap: 22px; font-size: 14px; }
  .nav nav a { text-decoration: none; color: var(--muted); }
  .nav nav a:hover, .nav nav a.here { color: var(--text); }
  .layout { display: grid; grid-template-columns: 240px minmax(0, 1fr); gap: 48px; padding: 40px 0 80px; }
  aside.toc { position: sticky; top: 84px; align-self: start; max-height: calc(100vh - 100px); overflow-y: auto; font-size: 13.5px; }
  .toc b { display: block; color: var(--muted); font-size: 12px; letter-spacing: 0.14em; margin-bottom: 8px; }
  .toc ol { list-style: none; margin: 0; padding: 0; }
  .toc li { margin: 2px 0; }
  .toc ol ol { padding-left: 14px; font-size: 12.5px; }
  .toc a { color: var(--muted); text-decoration: none; display: block; padding: 2px 0; }
  .toc a:hover { color: var(--text); }
  main { min-width: 0; }
  h1 {
    margin: 0 0 12px; font-size: clamp(30px, 5vw, 44px); font-weight: 800; letter-spacing: 0.03em; line-height: 1.25;
    background: var(--grad); -webkit-background-clip: text; background-clip: text; color: transparent;
  }
  .intro { color: var(--muted); }
  h2 { margin: 56px 0 14px; padding-top: 18px; border-top: 1px solid var(--line); font-size: 25px; }
  h3 { margin: 34px 0 10px; font-size: 19px; color: var(--cyan); }
  p, li { font-size: 15px; }
  ul, ol { padding-left: 1.4em; }
  li { margin: 4px 0; }
  strong { color: #fff; }
  code {
    font-family: "SF Mono", Menlo, Consolas, monospace; font-size: 0.86em; background: var(--bg-card);
    border: 1px solid var(--line); border-radius: 6px; padding: 1px 6px; word-break: break-all;
  }
  .note { margin: 20px 0; padding: 14px 18px; border-left: 3px solid var(--yellow); background: var(--bg-raised); border-radius: 0 12px 12px 0; font-size: 14.5px; }
  .table-scroll { overflow-x: auto; margin: 14px 0 18px; }
  table { width: 100%; border-collapse: collapse; font-size: 14px; }
  th, td { text-align: left; padding: 9px 12px; border-bottom: 1px solid var(--line); vertical-align: top; }
  th { color: var(--muted); font-weight: 600; font-size: 12.5px; letter-spacing: 0.06em; background: var(--bg-raised); }
  td:first-child { white-space: nowrap; }
  footer { border-top: 1px solid var(--line); padding: 30px 0 50px; color: var(--muted); font-size: 12.5px; }
  @media (max-width: 860px) {
    .layout { grid-template-columns: 1fr; gap: 0; }
    aside.toc { position: static; max-height: none; margin-bottom: 8px; padding: 16px 18px; background: var(--bg-raised); border-radius: 14px; }
    .toc ol ol { display: none; }
    .nav nav { gap: 14px; font-size: 13px; }
    .wrap { padding: 0 16px; }
    .wrap.layout { padding: 24px 16px 60px; }
    td:first-child { white-space: normal; }
  }
"""


def render(md: str) -> str:
    body, toc, title, intro = convert(md)
    toc_html = ["<ol>"]
    for text, anchor, subs in toc:
        toc_html.append('<li><a href="#%s">%s</a>' % (anchor, inline(text)))
        if subs:
            toc_html.append("<ol>" + "".join('<li><a href="#%s">%s</a></li>' % (a, inline(t)) for t, a in subs) + "</ol>")
        toc_html.append("</li>")
    toc_html.append("</ol>")
    intro_html = "".join("<p class=\"intro\">%s</p>" % inline(x) for x in intro)
    return """<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>%(title)s</title>
<meta name="description" content="FMLUME（6 オペレーター FM シンセサイザー）の使い方。インストール、画面の見方、Variations、Advanced の各タブ、プリセット、MIDI、DAW との連携。">
<link rel="icon" type="image/svg+xml" href="assets/icon.svg">
<!-- docs/USER_GUIDE.md から scripts/build_guide.py で作ったファイル。直接編集しない。 -->
<style>%(style)s</style>
</head>
<body>
<header class="nav">
  <div class="wrap">
    <a class="brand" href="./"><img src="assets/icon.svg" alt="">FMLUME</a>
    <nav>
      <a href="./">製品紹介</a>
      <a class="here" href="guide.html">使い方</a>
      <a href="%(repo)s">GitHub</a>
    </nav>
  </div>
</header>
<div class="wrap layout">
  <aside class="toc"><b>目次</b>%(toc)s</aside>
  <main>
    <h1>%(title)s</h1>
    %(intro)s
%(body)s
  </main>
</div>
<footer>
  <div class="wrap">
    <p>© 2026 CHEEBOW</p>
    <p>VST は Steinberg Media Technologies GmbH の商標です。Audio Units、macOS、Logic Pro は Apple Inc. の商標です。Cubase は Steinberg Media Technologies GmbH の登録商標です。</p>
  </div>
</footer>
</body>
</html>
""" % {"title": html.escape(title), "style": STYLE, "repo": REPO, "toc": "".join(toc_html), "intro": intro_html, "body": body}


def main() -> int:
    result = render(SRC.read_text(encoding="utf-8"))
    if "--check" in sys.argv:
        current = OUT.read_text(encoding="utf-8") if OUT.exists() else ""
        if current != result:
            print("site/guide.html is out of date: run python3 scripts/build_guide.py", file=sys.stderr)
            return 1
        print("site/guide.html is up to date")
        return 0
    OUT.write_text(result, encoding="utf-8")
    print("wrote", OUT.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
