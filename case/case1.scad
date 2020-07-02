include <BOSL/constants.scad>
use <BOSL/shapes.scad>
use <BOSL/transforms.scad>

SIZE_FRONT_X = 45;
SIZE_FRONT_Y = 15;
SIZE_FRONT_Z = 50;

rotate([-25, 0, 0])
difference() {
    cuboid([SIZE_FRONT_X, SIZE_FRONT_Y, SIZE_FRONT_Z], fillet=2, $fn=180);
    cuboid([SIZE_FRONT_X -4 , SIZE_FRONT_Y-4 , SIZE_FRONT_Z -4 ], fillet=2, $fn=180);
    // display
    translate([0, -SIZE_FRONT_Y/2, 5]) {
        cube([30, 5, 30], center=true);
    }
    // airflow for temperature sensor
    translate([0, 0, 15]) cube([SIZE_FRONT_X + 10, 1.5, 10], center=true);
    translate([0, 0, 0]) cube([SIZE_FRONT_X + 10, 1.5, 10], center=true);
}
