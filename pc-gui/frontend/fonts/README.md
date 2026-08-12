# fonts/

- `DSEG14Classic-Bold.woff2` - SIL OFL 1.1 (see `DSEG-LICENSE.txt`), committed
  normally. Used for the segment-style digit/character displays
  (`.digital-readout` / `.bttf-value`).

- `MicrogrammaDExtended-Bold.woff2` - **not committed**. Microgramma Bold
  Extended is a commercial Linotype/Monotype font and this repo is public,
  so the converted webfont is gitignored rather than redistributed. It's
  used site-wide (`style.css`'s `@font-face "Microgramma Bold Extended"`)
  for everything that isn't a segment-display digit.

  On a fresh clone/deploy, that font-face 404s and the site silently falls
  back to the next entry in the stack (system sans-serif) - it still works,
  just doesn't look right. To restore it:

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
