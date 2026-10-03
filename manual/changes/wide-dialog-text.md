---
title: Draw the dialog text with the wide Windows entry points
category: fix
release: 0.2.3
targets: []
credit: [OxFF]
---

The campaign list, the tooltips and the rest of the dialog text went through the ANSI GDI
calls, which read a string in the code page the font declared, so a cyrillic string came
out as mojibake on a system whose page is not the one the text was written in. Measuring
and drawing now convert the text to wide and call the wide entry points, and the dialog
font is a TrueType face that carries cyrillic glyphs.
