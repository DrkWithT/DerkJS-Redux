/* test else if's, etc. */

function xy_quadrant(x, y) {
    if (x > 0.0 && y > 0.0) {
        return 1;
    } else if (x < 0.0 && y > 0.0) {
        return 2;
    } else if (x < 0.0 && y < 0.0) {
        return 3;
    } else if (x > 0.0 && y < 0.0) {
        return 4;
    }

    return 0;
}

/* NOTE: This should print 4 */
return xy_quadrant(4.5, -1.5);
