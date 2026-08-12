# fonts/

- `DSEG14Classic-Bold.woff2` - SIL OFL 1.1 (see `DSEG-LICENSE.txt`), committed
  normally. Used for the segment-style digit/character displays
  (`.digital-readout` / `.bttf-value`).

- `Inter.woff2` - SIL OFL 1.1 (see `Inter-LICENSE.txt`), committed normally.
  The general UI font: headings, hints, buttons, tabs, form fields -
  everything except the three places below.

- `MicrogrammaDExtended-Bold.woff2` - **not committed**. Microgramma Bold
  Extended is a commercial Linotype/Monotype font and this repo is public,
  so the converted webfont is gitignored rather than redistributed. It's
  scoped to three hardware-nameplate spots (`style.css`'s
  `@font-face "Microgramma Bold Extended"`): the title bar (`.topbar h1`),
  the "Current Display Times" row labels (`.bttf-row-label`), and the
  digit-readout's own small labels (`.bttf-label` - Month/Day/Year/AM/PM/
  Hour/Min). Section headings (`.card h2`) and everything else use Inter.

  On a fresh clone/deploy, that font-face 404s and those three spots
  silently fall back to the next entry in the stack (system sans-serif) -
  still works, just doesn't look right. To restore it:

  1. Get a licensed copy of Microgramma D Extended Bold (`.otf`/`.ttf`).
  2. Convert it to woff2 (fontTools, already a dependency of
     `pc-gui/backend/.venv`):
     ```
     cd pc-gui/backend
     .venv/bin/python3 -c "
     from fontTools.ttLib import TTFont
     f = TTFont('/path/to/Microgramma D Extended Bold.otf')
     f.flavor = 'woff2'
     f.save('../frontend/fonts/MicrogrammaDExtended-Bold.woff2')
     "
     ```
  3. Deploy the resulting file to the Pi directly (scp/rsync), not via git -
     see the "Raspberry Pi deployment" section of `pc-gui/README.md`.
