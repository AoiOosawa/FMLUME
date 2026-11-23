#!/usr/bin/env python3
"""Generates FMLUME's logo / icon SVGs (assets/branding). Run from the repository root.

The mark is the series' rounded tile with three stacked sine waves: a cyan -> violet -> pink
centre sine over two offset ghost sines (orange, pink). It is the sine counterpart of
POLYBRISK's stacked saw waves and uses the same palette and stroke style.
The same wave formulas are used by ui::drawLogoMark (Source/UI/Common/Theme.cpp).
"""
import math, os

TAU = 2 * math.pi

def wave(kind, x):
    return math.sin(TAU * 2.0 * x)   # two clean sine periods (all three layers)

def path(kind, x0, x1, cy, amp, n=220):
    pts = []
    for i in range(n + 1):
        x = i / n
        pts.append(f"{x0 + x * (x1 - x0):.1f} {cy - wave(kind, x) * amp:.1f}")
    return "M" + " L".join(pts)

GRADS = """    <linearGradient id="fm" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#3cd6ff"/>
      <stop offset="0.55" stop-color="#8a6bff"/>
      <stop offset="1" stop-color="#d74db5"/>
    </linearGradient>"""

def mark(box_x, box_y, size):
    """Three stacked sine waves inside a square of `size` at (box_x, box_y): two ghosts and the hero."""
    x0, x1 = box_x + size * 0.17, box_x + size * 0.83
    cy = box_y + size * 0.5
    amp = size * 0.2
    dx, dy = size * 0.045, size * 0.085
    return f"""  <g fill="none" stroke-linejoin="round" stroke-linecap="round">
    <!-- ghost sines behind (offset up-left and down-right, like POLYBRISK's detuned saws) -->
    <path d="{path(0, x0 - dx, x1 - dx, cy - dy, amp)}" stroke="#ff6a3c" stroke-opacity="0.55" stroke-width="{size * 0.042:.1f}"/>
    <path d="{path(0, x0 + dx, x1 + dx, cy + dy, amp)}" stroke="#ff3c8e" stroke-opacity="0.5" stroke-width="{size * 0.042:.1f}"/>
    <!-- centre sine -->
    <path d="{path(0, x0, x1, cy, amp)}" stroke="url(#fm)" stroke-width="{size * 0.062:.1f}"/>
  </g>"""

def icon():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" width="1024" height="1024">
  <!-- FMLUME app icon. macOS grid: 824px rounded square on a 1024px canvas (same tile as POLYBRISK). -->
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2a2d4a"/>
      <stop offset="1" stop-color="#0d0e17"/>
    </linearGradient>
{GRADS}
    <radialGradient id="glow" cx="0.5" cy="0.45" r="0.6">
      <stop offset="0" stop-color="#6b7bff" stop-opacity="0.30"/>
      <stop offset="1" stop-color="#6b7bff" stop-opacity="0"/>
    </radialGradient>
  </defs>
  <rect x="100" y="100" width="824" height="824" rx="186" fill="url(#bg)"/>
  <rect x="100" y="100" width="824" height="824" rx="186" fill="url(#glow)"/>
  <rect x="103" y="103" width="818" height="818" rx="183" fill="none" stroke="#ffffff" stroke-opacity="0.08" stroke-width="6"/>
{mark(100, 100, 824)}
</svg>
"""

def logo_mark():
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 680 520" width="680" height="520">
  <!-- FMLUME symbol: three stacked sine waves. -->
  <defs>
{GRADS}
  </defs>
  <g transform="translate(-72 -152)">
{mark(0, 0, 824)}
  </g>
</svg>
"""

def wordmark(fill):
    return f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1060 260" width="1060" height="260">
  <!-- FMLUME horizontal logo. Text uses system fonts; outline it before release. -->
  <defs>
{GRADS}
  </defs>
  <g transform="translate(-40 -100) scale(0.56)">
{mark(0, 0, 824)}
  </g>
  <text x="430" y="168" font-family="'Avenir Next', 'Helvetica Neue', Helvetica, Arial, sans-serif"
        font-size="112" font-weight="800" font-style="italic" letter-spacing="6" fill="{fill}">FMLUME</text>
</svg>
"""

out = os.path.join("assets", "branding")
os.makedirs(out, exist_ok=True)
for name, text in (("icon.svg", icon()), ("logo-mark.svg", logo_mark()),
                   ("logo-wordmark-dark.svg", wordmark("#f4f5fb")), ("logo-wordmark-light.svg", wordmark("#14151f"))):
    with open(os.path.join(out, name), "w") as f:
        f.write(text)
print("wrote", out)
