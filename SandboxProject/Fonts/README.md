# Janus Pixel demo font

The atlas and Font v1 metrics are generated from the original 5×7 bitmap patterns
in [generate_demo_font.py](../../tools/generate_demo_font.py). No OS font, downloaded
font, or third-party font data is used. The patterns are project-authored source;
this change does not add a license or change the repository's licensing policy.

Rebuild from the repository root with `python tools/generate_demo_font.py`.
Only Python 3's standard library is needed; it is an offline authoring tool, not
an Engine dependency. The generated JSON and PNG are checked in for normal builds.

Coverage: A–Z, a–z (small-cap shapes), 0–9, space, and the punctuation listed in
the generator. Unsupported Unicode scalars display `?`; CR, LF and CRLF advance
one line. This demo font does not claim Chinese coverage, shaping or kerning.
The loader supports other offline atlases containing Unicode scalar glyphs.
