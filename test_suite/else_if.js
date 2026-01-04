/* test else if's, etc. */

function is_xy_quadrant_1_or_3(x, y) {
    if (x > 0.0 && y > 0.0) {
        return true;
    } else if (x < 0.0 && y < 0.0) {
        return true;
    } else return false;
}

/* NOTE: This should give false */
return is_xy_quadrant_1_or_3(4.5, -1.5);
