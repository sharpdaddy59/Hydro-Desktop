// hydro-dash-case.scad — parametric snap-fit enclosure for the
// Sunton ESP32-2432S028R "CYD" running hydro-dash firmware.
//
// Author note: every dimension that depends on the actual board is
// flagged "MEASURE" — take calipers to your unit before printing
// the final pieces. The defaults below come from web specs that
// disagreed on overall size, so the very first thing to do is print
// the back_shell at 0% infill, drop your bare board in, and confirm
// it slips into place with a comfortable ~0.4 mm gap.
//
// Construction:
//   - Two-piece body: back_shell (USB cutouts, kickstand sockets,
//     wall-mount keyhole) and front_bezel (screen window, LDR
//     pinhole). They join via cantilever snap-fit hooks around the
//     perimeter — no screws.
//   - PCB is captured by four alignment posts rising from the back
//     shell's internal floor, passing through the CYD's M2.5 holes
//     with a friction-rib fit, and seating into matching recesses
//     in the front bezel ceiling.
//   - Kickstand is a separate part that snaps into two slots on the
//     back. Pull it off to use the wall keyhole instead.
//
// Top-of-file `mode` selects what to render:
//   "front" / "back" / "kickstand" / "assembly" / "exploded"

mode = "exploded";

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
LCD_HEIGHT_ABOVE_PCB =  3.5;   // MEASURE: how far the screen sticks above the PCB
LCD_LEN              = 70.0;   // MEASURE: LCD module long edge
LCD_WID              = 50.5;   // MEASURE: LCD module short edge
LCD_OFFSET_X         =  4.0;   // MEASURE: LCD module's offset from PCB +X side
LCD_OFFSET_Y         = -0.25;  // MEASURE: LCD module sometimes overhangs PCB on Y

// Active glass area (the cutout in the front bezel)
SCREEN_ACTIVE_W      = 58.0;   // 2.8" 4:3 active area along PCB long edge
SCREEN_ACTIVE_H      = 43.5;
SCREEN_OFFSET_X      =  9.5;   // MEASURE: from PCB +X edge to active-area near edge
SCREEN_OFFSET_Y      =  3.5;   // MEASURE: from PCB -Y edge to active-area near edge

// ----- USB connectors (both on PCB +X short edge) -------------
// "Y position" = distance from PCB -Y edge to centre of connector.
// MEASURE all four; the defaults are guesses that need verification.
USB_C_WIDTH          =  9.4;
USB_C_HEIGHT         =  3.6;
USB_C_Y_CENTER       = 35.0;
USB_C_Z_OFFSET       = -2.0;   // centre relative to PCB BOTTOM surface (negative = below PCB)
MICRO_USB_WIDTH      =  8.4;
MICRO_USB_HEIGHT     =  3.4;
MICRO_USB_Y_CENTER   = 21.0;
MICRO_USB_Z_OFFSET   = -2.0;
USB_CUTOUT_SLACK     =  0.6;   // extra clearance around each USB cutout

// ----- LDR ----------------------------------------------------
LDR_HOLE_DIAM        =  2.0;
LDR_X_FROM_CORNER    = 14.0;   // MEASURE: position of LDR component on PCB front
LDR_Y_FROM_CORNER    =  3.5;

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

// ----- PCB-retention posts ------------------------------------
PCB_POST_DIAM        =  2.4;   // slip fit through PCB's mounting holes
PCB_POST_PROTRUDE    =  1.5;   // amount post sticks above PCB top into front bezel
PCB_POST_RIB_COUNT   =  4;     // axial friction ribs around the post
PCB_POST_RIB_OVERSIZE =  0.18; // each rib oversizes the post locally
PCB_POST_RIB_WIDTH   =  0.6;
POST_RECESS_DIAM     =  3.0;   // recess in front bezel that post tips seat into
POST_RECESS_DEPTH    =  2.0;

// ----- Snap-fit closure (front-to-back) -----------------------
// Hooks are on the FRONT bezel (cantilever beams hang down past the
// seam plane). Matching undercut lips are on the BACK shell. To open
// the case, insert a thin tool in the corner pry slot and twist.
SNAP_COUNT_LONG      =  2;     // hooks per long edge
SNAP_COUNT_SHORT     =  1;     // hook on the non-USB short edge
SNAP_HOOK_HEIGHT     =  1.0;   // protrusion (outward) of the hook tip
SNAP_HOOK_LENGTH     =  6.0;   // along the wall
SNAP_BEAM_THICKNESS  =  1.4;   // cantilever beam thickness (wall thickness in this region)
SNAP_BEAM_LENGTH     =  6.0;   // how far the beam hangs below the seam
SNAP_LIP_DEPTH       =  1.2;   // matching undercut depth on back shell
SNAP_LIP_FROM_TOP    =  3.0;   // distance from back shell's top edge to the lip
SNAP_LEAD_IN         =  0.6;   // bevel on hook face for engagement
PRY_SLOT_W           =  6.0;   // open slot at one corner for opening
PRY_SLOT_D           =  1.5;

// ----- Kickstand ----------------------------------------------
USE_KICKSTAND        = true;
KICKSTAND_ANGLE      = 18;     // degrees from vertical
KICKSTAND_WIDTH      = 30.0;
KICKSTAND_HEIGHT     = 35.0;
KICKSTAND_THK        =  3.0;
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

// PCB alignment post: cylinder + 4 axial friction ribs
module pcb_post(h_below_pcb, h_above_pcb) {
  // Main shaft (full height: floor to PCB top + protrude)
  total_h = h_below_pcb + PCB_THK + h_above_pcb;
  cylinder(d=PCB_POST_DIAM, h=total_h);
  // Friction ribs only in the through-PCB region (h_below_pcb..h_below_pcb+PCB_THK)
  rib_z0 = h_below_pcb;
  rib_z1 = h_below_pcb + PCB_THK;
  for (i = [0 : PCB_POST_RIB_COUNT - 1]) {
    rotate([0, 0, i * 360 / PCB_POST_RIB_COUNT])
      translate([PCB_POST_DIAM/2 - 0.05, -PCB_POST_RIB_WIDTH/2, rib_z0])
        cube([PCB_POST_RIB_OVERSIZE + 0.05,
              PCB_POST_RIB_WIDTH,
              rib_z1 - rib_z0]);
  }
  // Lead-in chamfer on top
  translate([0, 0, total_h - 0.6])
    cylinder(d1=PCB_POST_DIAM, d2=PCB_POST_DIAM - 0.6, h=0.6);
}

// One snap hook (cantilever beam pointing in -Z, hook face +X outward).
// Place against the inside of a +X facing wall; rotate as needed.
module snap_hook() {
  // Vertical beam (hangs down past the seam)
  cube([SNAP_BEAM_THICKNESS, SNAP_HOOK_LENGTH, SNAP_BEAM_LENGTH]);
  // Hook tip — small wedge protruding outward (-X direction in local frame)
  translate([-SNAP_HOOK_HEIGHT, 0, 0]) {
    // engagement face (top, undercut)
    cube([SNAP_HOOK_HEIGHT + 0.01, SNAP_HOOK_LENGTH, SNAP_LEAD_IN]);
    // lead-in ramp (slopes up)
    hull() {
      cube([0.01, SNAP_HOOK_LENGTH, SNAP_LEAD_IN]);
      translate([SNAP_HOOK_HEIGHT, 0, -SNAP_LEAD_IN])
        cube([0.01, SNAP_HOOK_LENGTH, 0.01]);
    }
  }
}

// Snap lip — undercut on the back shell that the hook seats into.
// Created via DIFFERENCE from the back shell wall.
module snap_lip_negative() {
  cube([SNAP_HOOK_HEIGHT + 0.4, SNAP_HOOK_LENGTH + 0.4, SNAP_LEAD_IN + 0.4]);
}

// Kickstand snap socket — slot in back shell back face.
module kickstand_socket_negative() {
  cylinder(d=KICKSTAND_PEG_DIAM + 0.3, h=KICKSTAND_PEG_LEN + 0.5);
}

// Wall-mount keyhole (subtractive). Cuts through back shell back wall.
module keyhole_negative() {
  // Big circle (head clearance) on top, slot extending down to small circle
  cylinder(d=KEYHOLE_DIAM_BIG, h=WALL + 1);
  translate([0, -KEYHOLE_SLOT_LEN, 0])
    cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
  hull() {
    translate([0, -KEYHOLE_SLOT_LEN, 0])
      cylinder(d=KEYHOLE_DIAM_SMALL, h=WALL + 1);
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
  difference() {
    union() {
      // Outer body
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP, SEAM_Z])
        rounded_box(OUTER_X, OUTER_Y, FRONT_DEPTH + WALL);

      // Snap hooks hanging down past the seam plane
      front_snap_hooks();
    }

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

    // Post-tip recesses at each PCB mount hole
    for (p = mount_hole_xy)
      translate([p[0], p[1], SEAM_Z + FRONT_DEPTH - POST_RECESS_DEPTH])
        cylinder(d=POST_RECESS_DIAM, h=POST_RECESS_DEPTH + 0.01);

    // Pry slot at one corner (BL) so a flat tool can split the seam
    translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP - 0.01,
               SEAM_Z - 0.01])
      cube([PRY_SLOT_W, WALL + 0.5, PRY_SLOT_D]);
  }
}

// Snap hooks attached to the inside of the front bezel walls,
// pointing down past the seam plane. Hooks engage matching lips on
// the back shell.
module front_snap_hooks() {
  // Long edges (-Y and +Y faces): SNAP_COUNT_LONG hooks each
  for (i = [0 : SNAP_COUNT_LONG - 1]) {
    x_off = PCB_LEN * (i + 1) / (SNAP_COUNT_LONG + 1)
            - SNAP_HOOK_LENGTH / 2;
    // -Y wall (inside surface faces +Y; hook protrudes -Y outward)
    translate([x_off, -PCB_PERIMETER_GAP - SNAP_BEAM_THICKNESS,
               SEAM_Z - SNAP_BEAM_LENGTH])
      mirror([1,0,0])
        rotate([0, 0, 90])
          snap_hook();
    // +Y wall
    translate([x_off + SNAP_HOOK_LENGTH,
               PCB_WID + PCB_PERIMETER_GAP + SNAP_BEAM_THICKNESS,
               SEAM_Z - SNAP_BEAM_LENGTH])
      rotate([0, 0, -90])
        snap_hook();
  }
  // Short edge: -X side only (the +X side has the USB cutouts)
  for (i = [0 : SNAP_COUNT_SHORT - 1]) {
    y_off = PCB_WID * (i + 1) / (SNAP_COUNT_SHORT + 1)
            - SNAP_HOOK_LENGTH / 2;
    translate([-PCB_PERIMETER_GAP - SNAP_BEAM_THICKNESS,
               y_off,
               SEAM_Z - SNAP_BEAM_LENGTH])
      snap_hook();
  }
}

// =============================================================
// BACK SHELL — cable side. USB cutouts, kickstand sockets, wall
// keyhole, vent slots, and the four PCB-retention posts.
// =============================================================
module back_shell() {
  difference() {
    union() {
      // Outer body
      translate([-WALL - PCB_PERIMETER_GAP, -WALL - PCB_PERIMETER_GAP, 0])
        rounded_box(OUTER_X, OUTER_Y, WALL + BACK_DEPTH + PCB_THK);

      // PCB-retention posts (rise from inner floor through PCB)
      for (p = mount_hole_xy)
        translate([p[0], p[1], WALL])
          pcb_post(BACK_DEPTH, PCB_POST_PROTRUDE);

      // Stylus channel housing — bump-out on the -Y long edge
      stylus_housing();
    }

    // Inner cavity (hollow tray)
    translate([-PCB_PERIMETER_GAP, -PCB_PERIMETER_GAP, WALL])
      cube([INNER_X, INNER_Y, BACK_DEPTH + PCB_THK + 0.01]);

    // USB-C cutout
    usb_cutout(USB_C_Y_CENTER, USB_C_WIDTH, USB_C_HEIGHT, USB_C_Z_OFFSET);
    // Micro-USB cutout
    usb_cutout(MICRO_USB_Y_CENTER, MICRO_USB_WIDTH, MICRO_USB_HEIGHT,
               MICRO_USB_Z_OFFSET);

    // Stylus channel (subtractive) — drilled into the housing bump-out
    stylus_channel();

    // Snap-lip undercuts (matching the front bezel's hooks)
    back_snap_lips();

    // Kickstand snap sockets on the back FACE (-Z face of the back shell)
    if (USE_KICKSTAND) back_kickstand_sockets();

    // Wall-mount keyhole on the back face
    back_keyhole();

    // Vent slot grid on the back face
    back_vents();
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

// Snap-lip undercuts in the back shell walls (matching front hooks)
module back_snap_lips() {
  for (i = [0 : SNAP_COUNT_LONG - 1]) {
    x_off = PCB_LEN * (i + 1) / (SNAP_COUNT_LONG + 1)
            - SNAP_HOOK_LENGTH / 2;
    // -Y wall
    translate([x_off - 0.2,
               -WALL - PCB_PERIMETER_GAP - 0.01,
               SEAM_Z - SNAP_LIP_FROM_TOP - SNAP_LEAD_IN - 0.2])
      cube([SNAP_HOOK_LENGTH + 0.4,
            SNAP_LIP_DEPTH + 0.01,
            SNAP_LEAD_IN + 0.4]);
    // +Y wall
    translate([x_off - 0.2,
               PCB_WID + PCB_PERIMETER_GAP + WALL - SNAP_LIP_DEPTH,
               SEAM_Z - SNAP_LIP_FROM_TOP - SNAP_LEAD_IN - 0.2])
      cube([SNAP_HOOK_LENGTH + 0.4,
            SNAP_LIP_DEPTH + 0.01,
            SNAP_LEAD_IN + 0.4]);
  }
  for (i = [0 : SNAP_COUNT_SHORT - 1]) {
    y_off = PCB_WID * (i + 1) / (SNAP_COUNT_SHORT + 1)
            - SNAP_HOOK_LENGTH / 2;
    translate([-WALL - PCB_PERIMETER_GAP - 0.01,
               y_off - 0.2,
               SEAM_Z - SNAP_LIP_FROM_TOP - SNAP_LEAD_IN - 0.2])
      cube([SNAP_LIP_DEPTH + 0.01,
            SNAP_HOOK_LENGTH + 0.4,
            SNAP_LEAD_IN + 0.4]);
  }
}

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
// back shell sockets; the foot leans at KICKSTAND_ANGLE.
// =============================================================
module kickstand() {
  difference() {
    union() {
      // Main body — angled wedge
      rotate([0, KICKSTAND_ANGLE, 0])
        translate([-KICKSTAND_WIDTH/2, -KICKSTAND_THK/2, 0])
          rounded_box(KICKSTAND_WIDTH, KICKSTAND_THK, KICKSTAND_HEIGHT,
                      r=1.0);
      // Two pegs on top
      translate([0, 0, KICKSTAND_HEIGHT - 0.5])
        for (dx = [-KICKSTAND_PEG_GAP/2, KICKSTAND_PEG_GAP/2])
          translate([dx, 0, 0])
            cylinder(d=KICKSTAND_PEG_DIAM, h=KICKSTAND_PEG_LEN);
    }
    // Finger relief slits beside each peg so the wings can flex
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
  if (USE_KICKSTAND)
    translate([PCB_LEN/2, -WALL - PCB_PERIMETER_GAP - 4, 0])
      rotate([90, 0, 180])
        kickstand();
}

module exploded_view() {
  pcb_dummy();
  back_shell();
  // Front bezel lifted up
  translate([0, 0, 30]) front_bezel();
  // Kickstand off to the side
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
