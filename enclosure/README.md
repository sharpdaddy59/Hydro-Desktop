# hydro-dash enclosure

Parametric OpenSCAD case for the hydro-dash dashboard running on a
Sunton ESP32-2432S028R "Cheap Yellow Display."

## Features

- Two-piece body, **friction-fit closure** — no screws, no inserts,
  no fasteners. The front bezel has hollow tubes that slide over
  the back shell's PCB-retention posts; friction in the bore holds
  the case closed. Tune `FRONT_BORE_DIAM` for the desired snap feel.
- **Stepped PCB-retention posts** — a wider shoulder under the PCB
  acts as a positive stop the board sits on. A narrower shaft above
  passes through the M2.5 mounting holes and protrudes up into the
  front bezel's bore for the friction fit.
- **Both USB ports optionally exposed** on one short edge (USB-C and
  Micro-USB), independently toggleable.
- **Optional microSD card-access slot** on the long edge opposite the
  USB connectors, so the card can be swapped without opening the case.
  Includes a finger-relief scoop for gripping a card that seats flush
  in the socket.
- **Optional integrated stylus channel** along one long edge with a
  thumb-grip notch at the far end.
- **Removable kickstand** for desk use, **wall keyhole** on the back
  for hanging when the kickstand is off.
- **LDR pinhole** on the front bezel (small enough not to dominate
  the look; useful on units where the LDR is functional).
- **Optional vent slots** on the back so the ESP32 doesn't cook.
- **Pry slot** at one corner of the seam for clean disassembly.

## Files

- [`hydro-dash-case.scad`](hydro-dash-case.scad) — the source. Open
  in OpenSCAD, set `mode` at the top of the file to `"front"`,
  `"back"`, `"kickstand"`, `"assembly"`, or `"exploded"`. Render
  (F6), export STL.
- `stl/` — bundled rendered parts and a Creality Ender-3 V3 slicer
  project. See [Quick print](#quick-print) below.

## Quick print

If you have a stock Sunton ESP32-2432S028R and don't want to touch
OpenSCAD, the rendered parts ship in `stl/`:

| File | Re-render with |
|------|----------------|
| `hydro-dash-case.3mf` | (sliced project — **Creality Ender-3 V3**) |
| `hydro-dash-back.stl` | `mode = "back"` |
| `hydro-dash-front.stl` | `mode = "front"` |
| `hydro-dash-kickstand.stl` | `mode = "kickstand"` |

**Ender-3 V3 owners:** open `hydro-dash-case.3mf` in Creality Print,
send to printer. All three parts in one job, **under an hour total**.
The back shell alone takes just under 30 minutes if you'd rather
fit-test it first before committing to the full print.

**Other printers:** slice the three STLs separately or together — see
the print settings in [Final print](#5-final-print) below.

If your CYD is a clone or a different revision (S028C capacitive,
non-standard PCB outline), skip the bundled files and re-render from
the SCAD source — the bundled geometry is dialed in for the S028R.

## Feature toggles

A block of booleans near the top of the SCAD file turns the optional
parts on or off without touching geometry:

```scad
USE_STYLUS     = true;   // integrated stylus channel on the long edge
USE_USB_C      = true;   // USB-C cutout on the short edge
USE_MICRO_USB  = true;   // Micro-USB cutout on the short edge
USE_KICKSTAND  = true;   // detachable rear kickstand + matching sockets
USE_BACK_VENTS = true;   // vents on the back face
USE_SD_SLOT    = true;   // microSD card-access slot on the -Y long wall
```

Set any to `false` and re-render — the geometry disappears with no
parameter cleanup needed.

**`USE_STYLUS` and `USE_SD_SLOT` share the -Y long wall** — the stylus
channel runs that wall's full length, so don't enable both. Pick one,
or relocate SD access by editing `sd_cutout()`.

## Workflow

### 1. Verify dimensions on YOUR board

The defaults in the parameter block are best-guesses from web specs
that disagreed on overall PCB size. Take calipers to your unit and
update at minimum:

- `PCB_LEN`, `PCB_WID`, `PCB_THK` — overall PCB size
- `MOUNT_HOLE_INSET` — distance from PCB edge to the centre of each
  M2.5 mounting hole
- `LCD_LEN`, `LCD_WID`, `LCD_OFFSET_X`, `LCD_OFFSET_Y` — the LCD
  module's footprint and its offset on the PCB
- `LCD_BEZEL_X_LEFT`, `LCD_BEZEL_X_RIGHT`, `LCD_BEZEL_Y` — the
  non-active border around the visible glass on each side of the
  LCD module. (X is split because the CYD's bezel is asymmetric on
  the long axis.) The active glass cutout is derived from these.
- `USB_C_Y_CENTER`, `MICRO_USB_Y_CENTER`, `*_Z_OFFSET`,
  `*_WIDTH`, `*_HEIGHT` — where each USB connector sits on the
  PCB +X short edge
- `SD_SLOT_X_CENTER`, `SD_SLOT_WIDTH`, `SD_SLOT_HEIGHT`,
  `SD_SLOT_Z_OFFSET` — where the microSD socket mouth sits on the
  PCB -Y long edge (the socket is on the *back* of the PCB)
- `LDR_X_FROM_CORNER`, `LDR_Y_FROM_CORNER` — visible LDR component

Every parameter that needs measuring is flagged `MEASURE` in a
comment in the SCAD file.

### 2. Render exploded view first

```scad
mode = "exploded";
```

Render (F6). You should see four parts (PCB dummy, back shell,
front bezel, kickstand) clearly separated. Confirm:

- The screen cutout in the front bezel lines up with the active glass
  region of the LCD on the PCB dummy.
- The USB cutouts in the back shell line up with where the connectors
  would be.
- The four front-bezel tubes sit directly above the four back-shell
  posts.

### 3. Fit-test the back shell

Render `mode = "back"`, export, slice at 0.3 mm layer height with
0% infill and 2 perimeters. Print the back shell only — total
print time ~30–40 min on a typical 200 mm/s printer.

Drop the bare PCB onto the four posts. The PCB should slide down the
shaft until it lands flat on the four shoulder tops — no force, no
wiggle. Check:

- **PCB sits too high above the cavity floor?** Reduce `BACK_DEPTH`
  (which sets the shoulder height too).
- **PCB tilted / shoulders not flush with PCB underside?** Verify
  `MOUNT_HOLE_INSET` matches your actual board.
- **PCB shaft doesn't fit through the M2.5 hole?** Reduce
  `PCB_POST_DIAM` (default 2.0 mm).
- **PCB shaft is too loose in the hole?** Increase `PCB_POST_DIAM`
  by 0.05 mm at a time.

### 4. Fit-test the front bezel

Render `mode = "front"`, slice the same way, print. With the back
shell + PCB still assembled, lower the front bezel so its tubes
align with the back-shell post tips and press down evenly. The
tubes should slide over the post tips with a firm friction grip.

- **Too loose / case falls open?** Reduce `FRONT_BORE_DIAM` by
  0.05 mm at a time.
- **Too tight / case won't close?** Increase `FRONT_BORE_DIAM`.
- **Case closes but won't open?** Verify the pry slot is reachable
  at one corner of the seam; deepen `PRY_SLOT_W` / `PRY_SLOT_D` if
  needed.

### 5. Final print

Once parameters are dialed in:

- **Material:** PETG preferred (better fatigue life and slight
  give for the friction fit). PLA+ also works.
- **Layer height:** 0.2 mm.
- **Infill:** 20% gyroid or grid.
- **Perimeters:** 3.
- **Print orientation:** front bezel face-down (the front face is
  the smoothest layer), back shell open-side-up.
- **Supports:** none needed.

Total final print: ~60–90 minutes for all three parts at 0.2 mm.
The bundled `.3mf` is profiled for the Creality Ender-3 V3 and
prints them in under an hour.

## Assembly

1. Drop the PCB onto the back shell's four posts. The board lands
   on the shoulder tops; the shaft passes through the mounting
   holes and protrudes a few millimetres above the PCB.
2. Lower the front bezel so the four tubes align with the four
   protruding post tips. Press evenly until the bezel is flush with
   the back shell rim — the friction in the bores should resist
   pulling apart.
3. (Optional) Push the kickstand pegs into the back-shell sockets
   until they bottom out.

## Disassembly

Insert a flat tool (small flathead screwdriver, plastic spudger)
into the **pry slot** at one corner of the seam. Twist gently to
spring the front-bezel tubes off the back-shell posts at the nearest
corner; the rest of the perimeter follows. PCB lifts off the posts
with finger pressure.

To remove the kickstand: squeeze the small finger-relief slits
beside each peg and pull straight away from the back shell.

## Wall mounting

Drive a #6 round-head wood screw into the wall, leaving the head
proud by ~3 mm. The keyhole on the back of the case slips over the
head (big circle on top), then the case slides down so the screw
shaft catches in the narrow slot. Confirm with a light tug before
letting go.

## Parameter reference

The full parameter block is at the top of `hydro-dash-case.scad`.
Group highlights:

| Group | Touches |
|-------|---------|
| `USE_*` | Feature toggles for optional parts |
| `PCB_*` | Board outline; affects every other piece's size |
| `LCD_*`, `LCD_BEZEL_*` | LCD module footprint and bezel widths (active-glass cutout is derived) |
| `USB_C_*`, `MICRO_USB_*` | Back-shell USB cutout positions and sizes |
| `SD_SLOT_*`, `SD_RELIEF_*` | microSD card-access slot + finger-relief scoop |
| `LDR_*` | Front-bezel pinhole position |
| `STYLUS_*` | Side channel diameter, length, retention notch |
| `PCB_POST_*` | Back-shell shoulder + shaft dimensions |
| `FRONT_POST_*`, `FRONT_BORE_*` | Front-bezel mating tube fit |
| `KICKSTAND_*` | Detachable stand geometry |
| `KEYHOLE_*` | Wall-mount slot dimensions |
| `PRY_SLOT_*` | Disassembly tool slot |
| `WALL`, `*_DEPTH`, `CASE_FILLET` | Overall body |

## Notes

- The SCAD source is the source of truth. The bundled `stl/` renders
  are convenience artifacts for the stock S028R; re-render from the
  SCAD after any parameter change rather than hand-editing the STLs.
- Community references (all STL-only):
  [mdkendall](https://www.printables.com/model/685845-enclosure-for-sunton-esp32-2432s028r-cheap-yellow-),
  [Michał USB-C remix](https://www.printables.com/model/744864-esp32-cheap-yellow-display-usb-c-version-enclosure),
  [DE_Markus](https://www.thingiverse.com/thing:6440252),
  [LeifA](https://www.thingiverse.com/thing:6485667).
