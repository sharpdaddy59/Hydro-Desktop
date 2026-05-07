// hydro-dash-case.scad — parametric friction-fit enclosure for the
// Sunton ESP32-2432S028R "CYD" running hydro-dash firmware.
//
// Author note: every dimension that depends on the actual board is
// flagged "MEASURE" — take calipers to your unit before printing
// the final pieces. The defaults below come from web specs that
// disagreed on overall size, so the very first thing to do is print
// the back_shell at 0% infill, drop your bare board in, and confirm
// it lands cleanly on the four post shoulders.
//
// Construction:
//   - Two-piece body: back_shell (USB cutouts, kickstand sockets,
//     wall-mount keyhole) and front_bezel (screen window, LDR
//     pinhole). They join via mating posts (front) and shafts (back)
//     with a tunable friction fit — no screws, no snap hooks.
//   - PCB is captured by four stepped posts rising from the
//     back-shell cavity floor: a wide shoulder under the PCB acts as
//     a positive vertical stop the board lands on, and a narrower
//     shaft above passes through the M2.5 mounting holes and
//     protrudes up into the front bezel's bores.
//   - Kickstand is a separate part that snaps into two sockets on
//     the back. Pull it off to use the wall keyhole instead.
//   - A pry slot at one corner of the seam accepts a flat tool to
//     re-open the case.
//
// Top-of-file `mode` selects what to render:
//   "front" / "back" / "kickstand" / "assembly" / "exploded"

mode = "exploded";

// =============================================================
// FEATURE TOGGLES — flip these to include/exclude optional parts.
// =============================================================
USE_STYLUS    = false;   // integrated stylus channel on the long edge
USE_USB_C     = true;   // USB-C cutout on the short edge
USE_MICRO_USB = false;   // Micro-USB cutout on the short edge
USE_KICKSTAND = true;   // detachable rear kickstand + matching sockets
USE_BACK_VENTS = false; // vents on back

// =============================================================
// PARAMETERS — tune these to match your board, your printer, and
// your stylus. Almost every issue you'll hit on first print is
// fixable here without touching the geometry below.
// =============================================================

// ----- PCB ----------------------------------------------------
PCB_LEN              = 86.0;   // MEASURE: long edge of the PCB
PCB_WID              = 50.0;   // MEASURE: short edge
PCB_THK              =  1.6;   // MEASURE: PCB thickness alone
MOUNT_HOLE_DIAM      =  2.7;   // through-hole diameter (PCB side)
MOUNT_HOLE_INSET     =  3.5;   // MEASURE: centre-of-hole to nearest PCB edge

// LCD module (the part that sticks up from the PCB front)
LCD_HEIGHT_ABOVE_PCB =  4.0;   // MEASURE: how far the screen sticks above the PCB
LCD_LEN              = 70.0;   // MEASURE: LCD module long edge
LCD_WID              = 50.5;   // MEASURE: LCD module short edge
LCD_OFFSET_X         =  9.0;   // MEASURE: LCD module's offset from PCB +X side
LCD_OFFSET_Y         = -0.25;  // MEASURE: LCD module sometimes overhangs PCB on Y

// LCD bezel — the non-active border around the visible glass on each
// side of the LCD MODULE. The active glass area is derived from the
// LCD module dimensions + these bezel widths, so you don't have to
// measure the active area separately. The X bezel is split LEFT/RIGHT
// because the CYD's LCD has asymmetric bezels on the long axis (one
// side of the active glass is much closer to the LCD module edge
// than the other — likely due to the ribbon connector layout).
//
// "LEFT" = the -X side of the PCB (away from USB).
// "RIGHT" = the +X side of the PCB (USB side).
LCD_BEZEL_X_LEFT     =  2.0;   // bezel between LCD's -X edge and active glass
LCD_BEZEL_X_RIGHT    =  8.0;   // bezel between LCD's +X edge and active glass
LCD_BEZEL_Y          =  3.5;   // bezel on short-axis sides (assumed symmetric)

// Derived: active glass area (the cutout in the front bezel)
SCREEN_ACTIVE_W      = LCD_LEN - LCD_BEZEL_X_LEFT - LCD_BEZEL_X_RIGHT;
SCREEN_ACTIVE_H      = LCD_WID - 2 * LCD_BEZEL_Y;
SCREEN_OFFSET_X      = LCD_OFFSET_X + LCD_BEZEL_X_LEFT;
SCREEN_OFFSET_Y      = LCD_OFFSET_Y + LCD_BEZEL_Y;

// ----- USB connectors (both on PCB +X short edge) -------------
// "Y position" = distance from PCB -Y edge to centre of connector.
// MEASURE all four; the defaults are guesses that need verification.
USB_C_WIDTH          =  9.4;
USB_C_HEIGHT         =  3.5;
USB_C_Y_CENTER       = 24.5;
USB_C_Z_OFFSET       = -2.0;   // centre relative to PCB BOTTOM surface (negative = below PCB)
MICRO_USB_WIDTH      =  8.0;
MICRO_USB_HEIGHT     =  3.5;
MICRO_USB_Y_CENTER   = 13.0;
MICRO_USB_Z_OFFSET   = -2.0;
USB_CUTOUT_SLACK     =  0.6;   // extra clearance around each USB cutout

// ----- LDR ----------------------------------------------------
LDR_HOLE_DIAM        =  2.0;
LDR_X_FROM_CORNER    =  5.0;   // MEASURE: position of LDR component on PCB front
LDR_Y_FROM_CORNER    =  12.0;

// ----- Stylus channel (on PCB -Y long edge) -------------------
STYLUS_DIAMETER      =  6.0;
STYLUS_LENGTH        = 100.0;  // total stylus length
STYLUS_FIT_GAP       =  0.4;
STYLUS_THUMB_NOTCH_W = 10.0;   // width of relief slot at the stylus's far end

// ----- Case body / fit ----------------------------------------
WALL                 =  2.4;
PCB_PERIMETER_GAP    =  0.4;   // case-to-PCB tolerance (each side)
FRONT_DEPTH          =  4.5;   // space above LCD top, before front bezel inner ceiling
BACK_DEPTH           =  6.5;   // space below PCB bottom for ESP-WROOM and connectors
CASE_FILLET          =  2.0;   // outer corner rounding
$fn                  = 64;

// ----- PCB-retention posts (back shell) -----------------------
// Stepped post: a wider SHOULDER below the PCB acts as a positive
// stop the board sits on (so you can't push it past — friction ribs
// turned out to be too easy to overshoot). The shoulder transitions
// into a narrower shaft that passes through the M2.5 mounting hole
// and continues up past the PCB top by PCB_POST_PROTRUDE — far
// enough for the front bezel's mating tubes to slide over and create
// the friction fit that holds the case closed.
//
// Cross-section profile:
//                             ___       <- chamfered tip
//                            |   |
//                            |   |       <- upper shaft (through PCB,
//                            |   |          PCB_POST_DIAM dia)
//                       _____|___|_____  <- PCB rests here on shoulder
//                      |               |
//                      |               | <- shoulder
//                      |               |    (PCB_POST_SHOULDER_DIAM dia)
//                      |               |
//                      ----------------- <- back-shell cavity floor
PCB_POST_SHOULDER_DIAM =  4.5; // wider seat the PCB rests on
PCB_POST_DIAM          =  2.0; // upper shaft, through-PCB hole fit
PCB_POST_PROTRUDE      =  4.0; // shaft extends above PCB top to engage front bore

// ----- Front bezel mating posts -------------------------------
// Hollow cylindrical tubes hanging from the front bezel ceiling, one
// per PCB mounting-hole position. Each slides OVER the protruding tip
// of the corresponding back-shell post. Friction between the bore
// and the back post shaft is what holds the case together — there
// are no snap hooks.
//
// Tune FRONT_BORE_DIAM until you get the click feel you want:
//   - too loose? decrease (toward PCB_POST_DIAM = 2.0)
//   - too tight? increase by 0.05 mm at a time
FRONT_POST_OD        =  4.5;   // outer diameter of the front post
FRONT_BORE_DIAM      =  2.1;   // bore diameter (default: light interference)
FRONT_POST_LEN       =  3.0;   // length the front post hangs below the ceiling
FRONT_BORE_TOP_GAP   =  1.0;   // solid material left at the top of the bore

// ----- Pry slot (for disassembly) -----------------------------
// Open slot at one corner of the seam — accepts a flat tool to twist
// the two halves apart when you need to re-open the case.
PRY_SLOT_W           =  6.0;
PRY_SLOT_D           =  1.5;

// ----- Kickstand ----------------------------------------------
// (USE_KICKSTAND lives in the FEATURE TOGGLES block at the top.)
KICKSTAND_ANGLE      = 18;     // degrees from vertical
KICKSTAND_WIDTH      = 30.0;
KICKSTAND_HEIGHT     = 35.0;
KICKSTAND_THK        =  3.5;
KICKSTAND_PEG_DIAM   =  3.5;
KICKSTAND_PEG_LEN    =  4.0;
KICKSTAND_PEG_GAP    = 22.0;   // distance between the two snap pegs

// ----- Wall mount keyhole -------------------------------------
KEYHOLE_DIAM_BIG     =  8.0;   // screw-head clearance
KEYHOLE_DIAM_SMALL   =  4.0;   // shaft slot
KEYHOLE_SLOT_LEN     =  8.0;
KEYHOLE_DEPTH        =  2.0;   // recess depth into back face for screw head
KEYHOLE_OFFSET_FROM_TOP = 14.0;

// ----- Vent slots --------------------------------------------
VENT_SLOT_LEN        = 18.0;
VENT_SLOT_W          =  1.5;
VENT_ROWS            =  2;
VENT_COLS            =  4;
VENT_PITCH_X         =  4.0;
VENT_PITCH_Y         =  3.0;

// =============================================================
// DERIVED CONSTANTS — usually no need to touch
// =============================================================
INNER_X              = PCB_LEN + 2*PCB_PERIMETER_GAP;
INNER_Y              = PCB_WID + 2*PCB_PERIMETER_GAP;
OUTER_X              = INNER_X + 2*WALL;
OUTER_Y              = INNER_Y + 2*WALL;
TOTAL_HEIGHT         = WALL + BACK_DEPTH + PCB_THK + LCD_HEIGHT_ABOVE_PCB
                     + FRONT_DEPTH + WALL;
SEAM_Z               = WALL + BACK_DEPTH + PCB_THK + LCD_HEIGHT_ABOVE_PCB;  // front meets back here

// Hole positions (PCB local coords). Order: BL, BR, TR, TL.
mount_hole_xy = [
  [MOUNT_HOLE_INSET,           MOUNT_HOLE_INSET],
  [PCB_LEN - MOUNT_HOLE_INSET, MOUNT_HOLE_INSET],
  [PCB_LEN - MOUNT_HOLE_INSET, PCB_WID - MOUNT_HOLE_INSET],
  [MOUNT_HOLE_INSET,           PCB_WID - MOUNT_HOLE_INSET],
];

// =============================================================
// HELPER MODULES
// =============================================================

// 2D rounded square -> extruded box with filleted vertical edges
module rounded_box(x, y, z, r=CASE_FILLET) {
  linear_extrude(z)
    offset(r=r) offset(r=-r) square([x, y]);
}

// PCB alignment post — stepped: wide shoulder below PCB, narrow shaft
// through PCB and above. The shoulder's top face is the surface the
// PCB rests on (positive vertical stop, can't be pushed past).
module pcb_post(h_below_pcb, h_above_pcb) {
  upper_h = PCB_THK + h_above_pcb;        // through PCB + protrude
  // Shoulder (below PCB)
  cylinder(d=PCB_POST_SHOULDER_DIAM, h=h_below_pcb);
  // Upper shaft (through PCB hole + protrude into front bore)
  translate([0, 0, h_below_pcb]) {
    // Straight section
    cylinder(d=PCB_POST_DIAM, h=upper_h - 0.6);
    // Chamfered tip — narrows from full diameter to a 1.4mm apex over
    // the top 0.6mm so the bore (and the PCB hole on insertion) can
    // self-centre on the post.
    translate([0, 0, upper_h - 0.6])
      cylinder(d1=PCB_POST_DIAM, d2=PCB_POST_DIAM - 0.6, h=0.6);
  }
}

// (Cantilever snap-fit geometry was removed in favour of the
// front-bezel mating tubes that slide over the back-shell PCB posts.
// See FRONT_POST_OD / FRONT_BORE_DIAM / FRONT_POST_LEN above and the
// front_bezel module below.)

// (snap_lip_negative removed — referenced parameters that were dropped
// when the cantilever snap-fit was replaced with the friction-post
// design.)

// Kickstand snap socket — slot in back shell back face.
module kickstand_socket_negative() {
  cylinder(d=KICKSTAND_PEG_DIAM + 0.3, h=KICKSTAND_PEG_LEN + 0.5);
}

// Wall-mount keyhole (subtractive). Cuts through back shell back wall.
// Orientation: big circle on top (where you slip the screw head in),
// slot extending DOWN into +Y so the case settles onto the screw via
// gravity and catches the shaft in the narrow slot.
module keyhole_negative() {
  // Big circle (head clearance) on top
  cylinder(d=KEYHOLE_DIAM_BIG, h=WALL + 1);
  // Slot extending in +Y to the small circle below
  translate([0, KEYHOLE_SLOT_LEN, 0])
    cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
  hull() {
    cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
    translate([0, KEYHOLE_SLOT_LEN, 0])
      cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
  }
  // Recess for screw head on the OUTER face of the back wall (so head sits flush)
  translate([0, 0, -0.01])
    cylinder(d=KEYHOLE_DIAM_BIG + 1, h=KEYHOLE_DEPTH);
}

// Vent slot grid (subtractive)
module vent_grid_negative() {
  for (cx = [0 : VENT_COLS - 1])
    for (cy = [0 : VENT_ROWS - 1])
      translate([cx * VENT_PITCH_X, cy * (VENT_SLOT_W + VENT_PITCH_Y), 0])
        cube([VENT_SLOT_LEN, VENT_SLOT_W, WALL + 1]);
}

// =============================================================
// PCB DUMMY (visualization only)
// =============================================================
module pcb_dummy() {
  color("green", 0.6) {
    // PCB itself
    translate([0, 0, WALL + BACK_DEPTH])
      cube([PCB_LEN, PCB_WID, PCB_THK]);
    // LCD module sitting on top of PCB
    translate([LCD_OFFSET_X, LCD_OFFSET_Y, WALL + BACK_DEPTH + PCB_THK])
      color("dimgray")
        cube([LCD_LEN, LCD_WID, LCD_HEIGHT_ABOVE_PCB]);
  }
}

// =============================================================
// FRONT BEZEL — viewer side. Has the screen cutout, LDR pinhole,
// post recesses, and snap hooks pointing down.
// =============================================================
module front_bezel() {
  union() {
    difference() {
      // Outer body
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP, SEAM_Z])
        rounded_box(OUTER_X, OUTER_Y, FRONT_DEPTH + WALL);

      // Screen cutout — through the front face
      translate([SCREEN_OFFSET_X, SCREEN_OFFSET_Y,
                 SEAM_Z + FRONT_DEPTH - 0.01])
        cube([SCREEN_ACTIVE_W, SCREEN_ACTIVE_H, WALL + 1]);

      // Inside cavity — hollow out the back side of the bezel
      translate([-PCB_PERIMETER_GAP, -PCB_PERIMETER_GAP, SEAM_Z - 0.01])
        cube([INNER_X, INNER_Y, FRONT_DEPTH + 0.02]);

      // LDR pinhole through the front face
      translate([LDR_X_FROM_CORNER, LDR_Y_FROM_CORNER,
                 SEAM_Z + FRONT_DEPTH - 0.01])
        cylinder(d=LDR_HOLE_DIAM, h=WALL + 1);

      // Pry slot at one corner so a flat tool can split the seam.
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP - 0.01,
                 SEAM_Z - 0.01])
        cube([PRY_SLOT_W, WALL + 0.5, PRY_SLOT_D]);
    }

    // Front mating posts — hollow cylinder tubes hanging from the
    // ceiling at each PCB mounting-hole position. Each tube slides over
    // the protruding tip of the back-shell PCB-retention post; friction
    // between the bore and the back post shaft holds the case closed.
    // Tune FRONT_BORE_DIAM (decrease for tighter fit) until you get
    // the snap feel you want.
    for (p = mount_hole_xy)
      translate([p[0], p[1], SEAM_Z + FRONT_DEPTH - FRONT_POST_LEN])
        difference() {
          cylinder(d=FRONT_POST_OD, h=FRONT_POST_LEN);
          // Bore: open at the bottom, closed FRONT_BORE_TOP_GAP from the top
          translate([0, 0, -0.01])
            cylinder(d=FRONT_BORE_DIAM,
                     h=FRONT_POST_LEN - FRONT_BORE_TOP_GAP + 0.01);
        }
  }
}

// (front_snap_hooks removed — friction-post design now used for case
// closure. See the front_bezel module's mating-post union for the
// replacement geometry.)

// =============================================================
// BACK SHELL — cable side. USB cutouts, kickstand sockets, wall
// keyhole, vent slots, and the four PCB-retention posts.
// =============================================================
module back_shell() {
  union() {
    // Outer body, stylus housing, and all subtractive features.
    // The PCB-retention posts are added AFTER this difference so the
    // cavity cube doesn't carve through them — they live inside the
    // cavity volume and would otherwise be wiped away.
    difference() {
      union() {
        // Outer body
        translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP, 0])
          rounded_box(OUTER_X, OUTER_Y, WALL + BACK_DEPTH + PCB_THK);

        // Stylus channel housing — bump-out on the -Y long edge
        if (USE_STYLUS) stylus_housing();
      }

      // Inner cavity (hollow tray)
      translate([-PCB_PERIMETER_GAP, -PCB_PERIMETER_GAP, WALL])
        cube([INNER_X, INNER_Y, BACK_DEPTH + PCB_THK + 0.01]);

      // USB-C cutout
      if (USE_USB_C)
        usb_cutout(USB_C_Y_CENTER, USB_C_WIDTH, USB_C_HEIGHT, USB_C_Z_OFFSET);
      // Micro-USB cutout
      if (USE_MICRO_USB)
        usb_cutout(MICRO_USB_Y_CENTER, MICRO_USB_WIDTH, MICRO_USB_HEIGHT,
                   MICRO_USB_Z_OFFSET);

      // Stylus channel (subtractive) — drilled into the housing bump-out
      if (USE_STYLUS) stylus_channel();

      // Kickstand snap sockets on the back FACE (-Z face of the back shell)
      if (USE_KICKSTAND) back_kickstand_sockets();

      // Wall-mount keyhole on the back face
      back_keyhole();

      // Vent slot grid on the back face
      if (USE_BACK_VENTS)
        back_vents();
    }

    // PCB-retention posts — rise from the cavity floor at z=WALL
    // through the PCB and protrude into the front bezel's recesses.
    // Added after the difference so the cavity cube doesn't truncate
    // them.
    for (p = mount_hole_xy)
      translate([p[0], p[1], WALL])
        pcb_post(BACK_DEPTH-1, PCB_POST_PROTRUDE);
  }
}

// USB connector cutout — runs through the +X short wall.
// y_center: PCB-local Y of cutout centre. z_offset: vertical
// position relative to PCB BOTTOM surface (negative = below PCB).
module usb_cutout(y_center, w, h, z_offset) {
  translate([PCB_LEN + PCB_PERIMETER_GAP - 0.5,
             y_center - w/2 - USB_CUTOUT_SLACK/2,
             WALL + BACK_DEPTH + z_offset - h/2 - USB_CUTOUT_SLACK/2])
    cube([WALL + 1.5,
          w + USB_CUTOUT_SLACK,
          h + USB_CUTOUT_SLACK]);
}

// Stylus channel: housing bump on the -Y wall + drilled bore
module stylus_housing() {
  // Bump runs along PCB long edge, on the outside of the -Y wall
  translate([(PCB_LEN - STYLUS_LENGTH * 0.9) / 2,
             -WALL - PCB_PERIMETER_GAP - (STYLUS_DIAMETER + 2*WALL),
             0])
    rounded_box(STYLUS_LENGTH * 0.9 + STYLUS_THUMB_NOTCH_W,
                STYLUS_DIAMETER + 2*WALL,
                WALL + BACK_DEPTH + PCB_THK,
                r=1.5);
}

module stylus_channel() {
  bore_d = STYLUS_DIAMETER + STYLUS_FIT_GAP;
  bore_y = -WALL - PCB_PERIMETER_GAP - WALL - bore_d/2;
  bore_z = (WALL + BACK_DEPTH + PCB_THK) / 2;

  // Main bore (open at the -X end so stylus slides in)
  translate([-WALL - PCB_PERIMETER_GAP - 0.5,
             bore_y, bore_z])
    rotate([0, 90, 0])
      cylinder(d=bore_d, h=STYLUS_LENGTH + 5);

  // Thumb-grip notch at the +X end (relief for pushing stylus out)
  translate([(PCB_LEN - STYLUS_THUMB_NOTCH_W),
             bore_y - bore_d/2 - 0.01, bore_z - bore_d/2])
    cube([STYLUS_THUMB_NOTCH_W,
          (STYLUS_DIAMETER + 2*WALL) - WALL,
          bore_d]);
}

// (back_snap_lips removed — no matching hook geometry to engage with
// under the friction-post design. The case is held by the back-shell
// PCB posts mating into the front-bezel bores.)

module back_kickstand_sockets() {
  // Two sockets along the back face's vertical centerline,
  // separated by KICKSTAND_PEG_GAP, centred horizontally.
  cx = PCB_LEN / 2;
  cy = PCB_WID / 2;
  for (dx = [-KICKSTAND_PEG_GAP/2, KICKSTAND_PEG_GAP/2])
    translate([cx + dx, cy, -0.01])
      kickstand_socket_negative();
}

module back_keyhole() {
  cx = PCB_LEN / 2;
  cy = PCB_WID - KEYHOLE_OFFSET_FROM_TOP;
  translate([cx, cy, -0.01])
    keyhole_negative();
}

module back_vents() {
  // Vent grid centred near the top half of the back face
  start_x = PCB_LEN / 2 - (VENT_COLS - 1) * VENT_PITCH_X / 2 - VENT_SLOT_LEN/2;
  start_y = PCB_WID / 2 + 8;
  translate([start_x, start_y, -0.01])
    vent_grid_negative();
}

// =============================================================
// KICKSTAND — separate snap-on part. Two pegs on top mate with the
// back shell sockets. Modeled FLAT for printability — the lean is
// applied only when shown in assembly_view.
// =============================================================
module kickstand() {
  difference() {
    union() {
      // Main body — flat slab, will be tilted via the wall's
      // socket angle once assembled.
      translate([-KICKSTAND_WIDTH/2, -KICKSTAND_THK/2, 0])
        rounded_box(KICKSTAND_WIDTH, KICKSTAND_THK, KICKSTAND_HEIGHT,
                    r=1.0);
      // Two pegs on top, attached to the body
      for (dx = [-KICKSTAND_PEG_GAP/2, KICKSTAND_PEG_GAP/2])
        translate([dx, 0, KICKSTAND_HEIGHT - 0.5])
          cylinder(d=KICKSTAND_PEG_DIAM, h=KICKSTAND_PEG_LEN);
    }
    // Finger relief slits beside each peg so the wings can flex when
    // squeezed for removal.
    for (dx = [-KICKSTAND_PEG_GAP/2, KICKSTAND_PEG_GAP/2])
      translate([dx, -KICKSTAND_THK, KICKSTAND_HEIGHT - 8])
        cube([0.8, 2*KICKSTAND_THK, 6]);
  }
}

// =============================================================
// LAYOUT MODES
// =============================================================
module assembly_view() {
  pcb_dummy();
  back_shell();
  front_bezel();
  // Kickstand: rotated and translated so it sits behind the case
  // leaning at KICKSTAND_ANGLE, pegs seated in the back-shell sockets.
  if (USE_KICKSTAND) {
    cx = PCB_LEN / 2;
    cy = PCB_WID / 2;
    translate([cx, cy, 0])
      rotate([90, 0, 0])              // stand it up
        rotate([0, KICKSTAND_ANGLE, 0])  // tilt back
          translate([0, 0, -KICKSTAND_PEG_LEN])
            kickstand();
  }
}

module exploded_view() {
  pcb_dummy();
  back_shell();
  // Front bezel lifted up
  translate([0, 0, 30]) front_bezel();
  // Kickstand off to the side, laid flat for clarity
  if (USE_KICKSTAND)
    translate([PCB_LEN + 30, PCB_WID/2, 0])
      kickstand();
}

// Top-level dispatch
if      (mode == "front")       front_bezel();
else if (mode == "back")        back_shell();
else if (mode == "kickstand")   kickstand();
else if (mode == "assembly")    assembly_view();
else if (mode == "exploded")    exploded_view();
else                            assembly_view();
