# hydro-dash enclosure

Parametric OpenSCAD case for the hydro-dash dashboard running on a
Sunton ESP32-2432S028R "Cheap Yellow Display."

## Features

- Two-piece **snap-fit** body — no screws, no inserts, no fasteners.
- **PCB retention** via four alignment posts that pass through the
  CYD's M2.5 mounting holes with friction ribs.
- **Both USB ports** exposed on one short edge (USB-C and Micro-USB).
- **Integrated stylus channel** along one long edge with a thumb-grip
  notch at the far end.
- **Removable kickstand** for desk use, **wall keyhole** on the back
  for hanging when the kickstand is off.
- **LDR pinhole** on the front bezel (small enough not to dominate
  the look; useful on units where the LDR is functional).
- **Vent slots** on the back so the ESP32 doesn't cook.

## Files

- [`hydro-dash-case.scad`](hydro-dash-case.scad) — the source. Open
  in OpenSCAD, set `mode` at the top of the file to `"front"`,
  `"back"`, `"kickstand"`, `"assembly"`, or `"exploded"`. Render
  (F6), export STL.
- `stl/` — gitignored. Your local renders go here.

## Workflow

### 1. Verify dimensions on YOUR board

The defaults in the parameter block are best-guesses from web specs
that disagreed on overall PCB size. Take calipers to your unit and
update at minimum:

- `PCB_LEN`, `PCB_WID`, `PCB_THK` — overall PCB size
- `MOUNT_HOLE_INSET` — distance from PCB edge to the centre of each
  M2.5 mounting hole
- `LCD_*` — the LCD module's footprint and its offset on the PCB
- `SCREEN_OFFSET_X`, `SCREEN_OFFSET_Y` — where the active glass area
  sits relative to PCB corners
- `USB_C_Y_CENTER`, `MICRO_USB_Y_CENTER`, `*_Z_OFFSET` — where each
  USB connector sits on the PCB +X short edge
- `LDR_X_FROM_CORNER`, `LDR_Y_FROM_CORNER` — visible LDR component

Every parameter that needs measuring is flagged `MEASURE` in a
comment in the SCAD file.

### 2. Render exploded view first

```
mode = "exploded";
```

Render (F6). You should see four parts (PCB dummy, back shell,
front bezel, kickstand) clearly separated. Confirm the screen cutout
in the front bezel lines up with the LCD on the PCB dummy and that
the USB cutouts in the back shell line up with where the connectors
would be.

### 3. Print a fit-test

Render `mode = "back"`, export, slice at 0.3 mm layer height with
0 % infill and 2 perimeters. Print the back shell only — total
print time ~30-40 min.

Drop the bare PCB onto the four alignment posts. Should slide on
with firm finger pressure, no force.

- **Too tight?** Reduce `PCB_POST_RIB_OVERSIZE` by 0.05 mm.
- **Too loose / wobbles?** Increase by 0.05 mm.
- **PCB sits too high / posts protrude past PCB?** Reduce
  `PCB_POST_PROTRUDE`.

Reprint the back shell only until fit is right.

### 4. Print front bezel and check snap-fit

Render `mode = "front"`, slice and print at the same low-quality
draft settings. Try snapping the front onto the back-with-PCB-already-
seated.

- **Won't snap together?** Increase `SNAP_LIP_DEPTH` by 0.1 mm or
  decrease `SNAP_HOOK_HEIGHT` by 0.1 mm.
- **Too loose / pops apart easily?** Reverse the above.
- **Snaps OK but won't open?** Verify the pry slot is reachable.
  You can deepen `PRY_SLOT_W` and `PRY_SLOT_D`.

### 5. Final print

Once parameters are dialed in:

- **Material:** PETG preferred (better living-hinge / snap fatigue
  life than PLA). PLA+ also works.
- **Layer height:** 0.2 mm.
- **Infill:** 20 % gyroid or grid.
- **Perimeters:** 3.
- **Print orientation:** front bezel face-down, back shell open-side-
  up. The cantilever beams on the front bezel print best when the
  beam axis is vertical so the engagement face isn't a layer-shear
  weak point — the slicer should show this naturally for the
  "front bezel face-down" orientation.
- **Supports:** none needed if you orient as above.

Total final print: ~3-4 hours for both halves on a typical 200 mm/s
printer at 0.2 mm.

## Assembly

1. Drop the PCB onto the back shell's four posts. Press evenly until
   the PCB seats against the post shoulders.
2. Lower the front bezel onto the back shell. Align the screen
   cutout over the LCD. Press the long edges down evenly until the
   four snap clips click. The seam should disappear into a thin,
   even line.
3. (Optional) Push the kickstand pegs into the back shell sockets
   until they bottom out.

## Disassembly

Insert a flat tool (small flathead screwdriver, plastic spudger) into
the **pry slot** at the lower-left corner of the seam. Twist gently
to spring the nearest snap clip out of its lip, then walk around the
perimeter releasing the others. PCB lifts off the posts with finger
pressure.

To remove the kickstand: squeeze the small finger-relief slits on
either side of each peg and pull straight away from the back shell.

## Wall mounting

Drive a #6 round-head wood screw into the wall, leaving the head
proud by ~3 mm. The keyhole on the back of the case slips over the
head, then the case slides down to lock in the slot. Confirm with
a light tug before letting go.

## Parameter reference

The full parameter block is at the top of `hydro-dash-case.scad`.
Group highlights:

| Group | Touches |
|-------|---------|
| `PCB_*` | Board outline; affects every other piece's size |
| `LCD_*`, `SCREEN_*` | Front bezel screen cutout |
| `USB_*` | Back shell USB cutouts |
| `LDR_*` | Front bezel pinhole position |
| `STYLUS_*` | Side channel diameter, length, retention notch |
| `PCB_POST_*` | PCB retention fit |
| `SNAP_*` | Snap-fit clip behavior |
| `KICKSTAND_*` | Detachable stand geometry |
| `KEYHOLE_*` | Wall-mount slot dimensions |
| `WALL`, `*_DEPTH`, `CASE_FILLET` | Overall body |

## Notes

- The bundled STL renders are intentionally gitignored — the SCAD
  source is the source of truth. Re-render after any parameter
  change.
- Community references (all STL-only):
  [mdkendall](https://www.printables.com/model/685845-enclosure-for-sunton-esp32-2432s028r-cheap-yellow-),
  [Michał USB-C remix](https://www.printables.com/model/744864-esp32-cheap-yellow-display-usb-c-version-enclosure),
  [DE_Markus](https://www.thingiverse.com/thing:6440252),
  [LeifA](https://www.thingiverse.com/thing:6485667).
