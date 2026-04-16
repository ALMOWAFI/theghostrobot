// ═══════════════════════════════════════════════════════════════════
//  THE GHOST ROBOT — Flipper Arm  (v2)
//  ─────────────────────────────────────
//  Pivot-lever wedge for 25mm stroke linear actuator
//
//  MECHANISM:
//    • Pivot bolt fixed to chassis side walls (M5)
//    • Tip rests 0.5mm off ground — scoops under any opponent
//    • Actuator clevis connects at rear of arm (below pivot axis)
//    • Actuator extends → rear pushed → tip rises ~50mm
//    • Pivot at 2/3 arm length = max mechanical advantage
//
//  SIDE VIEW (at rest):
//
//      [chassis]──────────────────────────────────
//            ●  ← pivot bolt (M5, through ears)
//             ╲
//              ╲  arm
//               ╲
//        ACT═════●  ← clevis slot (actuator rod end)
//                 ╲
//      ─────────────╲__ tip (0.5mm from ground)
//
//  SIDE VIEW (triggered):
//
//      [chassis]
//            ●
//           ╱
//          ╱  arm rises
//         ╱
//   ACT→→●  (pushed forward by actuator)
//
//  PRINT SETTINGS:
//    Material   : PETG
//    Layer      : 0.2mm
//    Infill     : 40% gyroid
//    Perimeters : 4
//    Supports   : None needed (print flat, bottom face down)
//
//  HARDWARE:
//    Pivot  : M5 × 40mm bolt + M5 nyloc nut (through chassis walls + ears)
//    Actuator: M5 × 20mm bolt through clevis slot → actuator rod clevis end
//
//  PREVIEW ANIMATION:
//    In OpenSCAD → View → Animate
//    Set FPS=10, Steps=50 to see the flip motion
// ═══════════════════════════════════════════════════════════════════

// ─── Parameters ──────────────────────────────────────────────────
TOTAL_LEN   = 160;   // mm — total arm length (tip to rear)
PIVOT_FRAC  = 0.67;  // pivot at 2/3 from tip = max leverage (do not change)

ARM_WIDTH   = 178;   // mm — arm width (fits 200mm robot with 1mm clearance each side)
THICK_ROOT  = 13;    // mm — arm thickness at pivot and rear (structural zone)
THICK_TIP   = 0.5;   // mm — wedge tip thickness (0.5mm = scoops under any opponent)

EAR_DEPTH   = 10;    // mm — ear extension beyond arm width (bolt clearance)
EAR_LEN     = 24;    // mm — ear length along arm axis

PIVOT_D     = 5.4;   // mm — M5 pivot bolt hole
CLEVIS_W    = 6.0;   // mm — clevis slot width (actuator rod end pin diameter + 0.5mm)
CLEVIS_H    = 12;    // mm — clevis slot height (allows actuator angle change during rotation)
CLEVIS_WALL = 4;     // mm — wall thickness around clevis slot

RIB_H       = 10;    // mm — rib height above arm top surface
RIB_T       = 3;     // mm — rib thickness

// Actuator stroke (for animation)
ACT_STROKE  = 25;    // mm — linear actuator stroke

// ─── Computed values ─────────────────────────────────────────────
$fn = 60;

PIVOT_X     = TOTAL_LEN * PIVOT_FRAC;  // mm from tip to pivot
ARM_REAR    = TOTAL_LEN - PIVOT_X;     // mm from pivot to rear

// Actuator clevis at 75% into rear section (away from pivot for more torque)
CLEVIS_X    = PIVOT_X + ARM_REAR * 0.75;
CLEVIS_Z    = THICK_ROOT * 0.5;        // mid-height of rear section

// Animation: rotate arm around pivot using $t
FLIP_ANGLE  = 55;   // degrees max flip angle
anim_angle  = $t * FLIP_ANGLE;

echo("=== FLIPPER ARM ===");
echo(str("Total length  : ", TOTAL_LEN, " mm"));
echo(str("Pivot position: ", PIVOT_X,   " mm from tip  (", PIVOT_FRAC*100, "%)"));
echo(str("Rear section  : ", ARM_REAR,  " mm"));
echo(str("Arm width     : ", ARM_WIDTH, " mm"));
echo(str("Tip thickness : ", THICK_TIP, " mm"));
echo(str("Clevis X      : ", CLEVIS_X,  " mm from tip"));

// ─── Modules ─────────────────────────────────────────────────────

// Main wedge body
// Bottom face flat (Z=0), top face tapers tip→root
module arm_body() {
    hull() {
        // Tip — paper thin
        cube([1, ARM_WIDTH, THICK_TIP]);
        // At pivot — full thickness
        translate([PIVOT_X, 0, 0])
            cube([1, ARM_WIDTH, THICK_ROOT]);
        // At rear — full thickness
        translate([TOTAL_LEN, 0, 0])
            cube([1, ARM_WIDTH, THICK_ROOT]);
    }
}

// Pivot ears — solid blocks extending beyond arm width
module pivot_ears() {
    for (side = [[-EAR_DEPTH, 0], [ARM_WIDTH, 0]]) {
        translate([PIVOT_X - EAR_LEN/2, side[0], 0])
            cube([EAR_LEN, EAR_DEPTH, THICK_ROOT]);
    }
}

// Clevis bracket — reinforced block around actuator connection point
// Actuator rod end slides in and is pinned with M5 bolt through clevis slot
module clevis_bracket() {
    bw = ARM_WIDTH * 0.3;   // bracket width (centered on arm)
    by = (ARM_WIDTH - bw) / 2;
    bx = CLEVIS_X - CLEVIS_W/2 - CLEVIS_WALL;
    blen = CLEVIS_W + CLEVIS_WALL * 2;

    translate([bx, by, 0])
        cube([blen, bw, THICK_ROOT + CLEVIS_H/2]);
}

// Clevis slot — elongated hole for actuator rod pin
// Slot is taller than wide to allow angle change as arm rotates
module clevis_cut() {
    bw = ARM_WIDTH * 0.3;
    by = (ARM_WIDTH - bw) / 2;

    translate([CLEVIS_X, by - 1, CLEVIS_Z])
        rotate([-90, 0, 0])
            hull() {
                cylinder(d = CLEVIS_W, h = bw + 2);
                translate([0, CLEVIS_H * 0.5, 0])
                    cylinder(d = CLEVIS_W, h = bw + 2);
                translate([0, -CLEVIS_H * 0.3, 0])
                    cylinder(d = CLEVIS_W, h = bw + 2);
            }
}

// Longitudinal ribs — stiffens the wide arm against flex
module ribs() {
    rib_ys = [ARM_WIDTH * 0.2, ARM_WIDTH * 0.5, ARM_WIDTH * 0.8];

    for (y = rib_ys) {
        hull() {
            translate([TOTAL_LEN * 0.15, y - RIB_T/2, THICK_TIP])
                cube([1, RIB_T, 1]);
            translate([PIVOT_X, y - RIB_T/2, THICK_ROOT])
                cube([1, RIB_T, RIB_H]);
            translate([TOTAL_LEN, y - RIB_T/2, THICK_ROOT])
                cube([1, RIB_T, RIB_H]);
        }
    }
}

// Bottom front chamfer — helps tip slide under opponent
module tip_chamfer() {
    c = THICK_ROOT * 0.8;
    translate([0, -1, 0])
        linear_extrude(ARM_WIDTH + 2)
            polygon([[0,0],[c,0],[0,c]]);
}

// ─── Static arm geometry ─────────────────────────────────────────
module flipper_arm() {
    difference() {
        union() {
            arm_body();
            pivot_ears();
            clevis_bracket();
            ribs();
        }
        // Pivot hole — runs full width + ears
        translate([PIVOT_X, -EAR_DEPTH - 1, THICK_ROOT / 2])
            rotate([-90, 0, 0])
                cylinder(d = PIVOT_D, h = ARM_WIDTH + EAR_DEPTH*2 + 2);
        // Clevis slot
        clevis_cut();
        // Tip chamfer
        tip_chamfer();
    }
}

// ─── ANIMATION — pivot around bolt axis ──────────────────────────
// Rotate arm around pivot point to preview flip motion
// In OpenSCAD: View → Animate, FPS=10, Steps=50
translate([PIVOT_X, 0, THICK_ROOT/2])
    rotate([0, -anim_angle, 0])
        translate([-PIVOT_X, 0, -THICK_ROOT/2])
            flipper_arm();
