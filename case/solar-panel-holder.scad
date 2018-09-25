union() {
	cube([3, 150, 3]);
	translate([0, 0, -3]) {
		cube([3, 3, 3]);
	}
	translate([0, 147, -3]) {
		cube([3, 3, 3]);
	}
	translate([0, 25, -35]) {
		cube([3, 3, 35]);
	}
	translate([0, 122, -35]) {
		cube([3, 3, 35]);
	}
	translate([0, 25, -38]) {
		cube([3, 100, 3]);
	}
};