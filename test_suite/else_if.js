/* test else if's, etc. */

function get_quad(x, y) {
    if (x > 0.0 && y > 0.0) {
        return 1;
    } else if (x < 0.0 && y > 0.0) {
        return 2;
    } else if (x < 0.0 && y < 0.0) {
        return 3;
    } else {
        return 4;
    }
}

/* NOTE: This should give 3 */
console.log(get_quad(-4.5, -1.5));

return 0;
