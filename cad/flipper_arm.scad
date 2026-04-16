// ═══════════════════════════════════════════════════════════════════
//  THE GHOST ROBOT — Flipper Arm
//  ─────────────────────────────
//  Pivot-lever wedge flipper for 25mm linear actuator
//
//  HOW IT WORKS:
//    1. Pivot bolt runs through the ears → fixed to chassis side walls
//    2. Arm rests with tip at ground level (~2mm clearance)
//    3. Actuator rod connects through the rear hole (bolt in Y direction)
//    4. Actuator extends → pushes rear section → tip rises → flips opponent
//
//  Side view:
//
//    [chassis front]
//         |
//    ─────●──────────────────── tip  ← resting position (wedge low)
//    pivot↑
//         |
//    ACT══● ← actuator pushes here (below pivot on rear section)
//
//    When ACT extends:
//    tip ╱╱╱╱  ← rises up ~50mm from 25mm actuator stroke
//
//  PRINT SETTINGS:
//    Material   : PETG
//    Layer      : 0.2mm
//    Infill     : 40% gyroid
//    Perimeters : 4
//    Orientation: flat (bottom face down, no supports needed)
//
//  HARDWARE:
//    Pivot  : M5 bolt × 2 + M5 nuts (through chassis side walls + ears)
//    Actuator: M5 bolt through rear hole → actuator rod end clevis
// ═══════════════════════════════════════════════════════════════════

// ─── Parameters — adjust these to fit your robot ─────────────────

ARM_FWD     = 110;   // mm — pivot to tip (forward arm length)
ARM_REAR    = 50;    // mm — pivot to back (lever arm for actuator)
ARM_WIDTH   = 178;   // mm — full arm width (fits inside 200mm robot)

THICK_ROOT  = 12;    // mm — arm thickness at pivot + rear (structural)
THICK_TIP   = 2;     // mm — wedge tip thickness (gets under opponent)

EAR_DEPTH   = 10;    // mm — how far each ear extends beyond arm edge
EAR_LENGTH  = 22;    // mm — ear length along X axis (around pivot)

PIVOT_D     = 5.4;   // mm — pivot bolt hole (M5 clearance = 5.4mm)
ACT_D       = 5.4;   // mm — actuator bolt hole (M5 clearance)

// Actuator hole position within rear section
// X: how far from rear end (0 = at rear, 1 = at pivot)
// Z: height fraction of arm thickness (0 = bottom, 1 = top)
ACT_X_FRAC  = 0.5;   // 50% into rear section
ACT_Z_FRAC  = 0.35;  // 35% height = lower portion (below pivot center)

RIB_H       = 9;     // mm — rib height above arm top face
RIB_T       = 3;     // mm — rib thickness

// ─────────────────────────────────────────────────────────────────
$fn = 60;

// Computed positions
PIVOT_X  = ARM_FWD;
REAR_END = ARM_FWD + ARM_REAR;
ACT_X    = ARM_FWD + ARM_REAR * ACT_X_FRAC;
ACT_Z    = THICK_ROOT * ACT_Z_FRAC;

// ─── Main arm body ────────────────────────────────────────────────
// Bottom face is flat at Z=0 (rests on ground when retracted)
// Top face tapers: full thickness at pivot/rear, thin at tip
module arm_body() {
    hull() {
        // Tip — thin wedge edge
        cube([1, ARM_WIDTH, THICK_TIP]);

        // At pivot — full thickness
        translate([PIVOT_X, 0, 0])
            cube([1, ARM_WIDTH, THICK_ROOT]);

        // At rear end — full thickness
        translate([REAR_END, 0, 0])
            cube([1, ARM_WIDTH, THICK_ROOT]);
    }
}

// ─── Pivot ears ───────────────────────────────────────────────────
// Solid blocks extending beyond arm width on both sides
// The pivot bolt passes through these + the arm body
module pivot_ears() {
    // Left ear (Y < 0)
    translate([PIVOT_X - EAR_LENGTH/2, -EAR_DEPTH, 0])
        cube([EAR_LENGTH, EAR_DEPTH, THICK_ROOT]);

    // Right ear (Y > ARM_WIDTH)
    translate([PIVOT_X - EAR_LENGTH/2, ARM_WIDTH, 0])
        cube([EAR_LENGTH, EAR_DEPTH, THICK_ROOT]);
}

// ─── Reinforcing ribs ─────────────────────────────────────────────
// Longitudinal ribs on the top face for stiffness
// Tapers with the arm: low at tip, full height at pivot and rear
module ribs() {
    rib_ys = [ARM_WIDTH * 0.25, ARM_WIDTH * 0.5, ARM_WIDTH * 0.75];

    for (y = rib_ys) {
        hull() {
            // Near tip — very short rib starts here
            translate([ARM_FWD * 0.25, y - RIB_T/2, THICK_TIP])
                cube([1, RIB_T, RIB_H * 0.2]);

            // At pivot — full rib height
            translate([PIVOT_X, y - RIB_T/2, THICK_ROOT])
                cube([1, RIB_T, RIB_H]);

            // At rear — full rib height
            translate([REAR_END, y - RIB_T/2, THICK_ROOT])
                cube([1, RIB_T, RIB_H]);
        }
    }
}

// ─── Wedge tip chamfer ────────────────────────────────────────────
// Small 45° chamfer on the bottom front edge
// Helps the arm slide under the opponent smoothly
module tip_chamfer() {
    chamfer = THICK_TIP * 1.5;
    translate([0, -1, 0])
        rotate([0, 0, 0])
            // Cut a triangular prism from the very front bottom edge
            linear_extrude(height = ARM_WIDTH + 2)
                polygon([[0, 0], [chamfer, 0], [0, chamfer]]);
}

// ─── Final model ──────────────────────────────────────────────────
difference() {
    union() {
        arm_body();
        pivot_ears();
        ribs();
    }

    // Pivot hole — through full arm width + both ears, at mid-thickness
    translate([PIVOT_X, -EAR_DEPTH - 1, THICK_ROOT / 2])
        rotate([-90, 0, 0])
            cylinder(d = PIVOT_D, h = ARM_WIDTH + EAR_DEPTH * 2 + 2);

    // Actuator hole — runs full width for flexible rod-end placement
    translate([ACT_X, -1, ACT_Z])
        rotate([-90, 0, 0])
            cylinder(d = ACT_D, h = ARM_WIDTH + 2);

    // Tip chamfer — bottom front edge
    tip_chamfer();
}

// ─── Dimensions echo (visible in OpenSCAD console) ───────────────
echo("=== FLIPPER ARM DIMENSIONS ===");
echo(str("Width        : ", ARM_WIDTH, " mm"));
echo(str("Fwd length   : ", ARM_FWD,   " mm  (pivot to tip)"));
echo(str("Rear length  : ", ARM_REAR,  " mm  (pivot to actuator)"));
echo(str("Total length : ", ARM_FWD + ARM_REAR, " mm"));
echo(str("Tip thickness: ", THICK_TIP, " mm"));
echo(str("Root thick   : ", THICK_ROOT," mm"));
echo(str("Pivot hole X : ", PIVOT_X,   " mm from tip"));
echo(str("Actuator X   : ", ACT_X,     " mm from tip"));
echo(str("Actuator Z   : ", ACT_Z,     " mm from bottom"));
echo("Pivot bolt  : M5");
echo("Actuator bolt: M5");
echo("Material    : PETG, 40% gyroid, 4 perimeters");
