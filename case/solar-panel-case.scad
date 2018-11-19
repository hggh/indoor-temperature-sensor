union() {
	difference() {
		translate([0, -2, 0]) {
			cube([116, 2, 66]);
		}
		translate([4, -2, 4]) {
			cube([108, 2, 58]);
		}
	}
	difference() {
		cube([116, 6, 66]);
		translate([2, 0, 2]) {
			cube([112, 4, 62]);
		}
		translate([]) {
			cube([2, 4, 64]);
		}
	}
	translate([-20, 5, 30]) {
		cube([156, 3, 3]);
	}
	translate([-20, 5, 27]) {
		cube([3, 3, 3]);
	}
	translate([133, 5, 27]) {
		cube([3, 3, 3]);
	}
}