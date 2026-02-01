var a = 1;
// TEST 1:
// a = 2;

// TEST 2:
var
// this should skip
b = 68;

if (a == 2) {
    return 1;
}

if (a + b != 69) {
    return 1;
}

// TEST 3:
/* / * a = 0; * / */

if (a == 0) {
    return 1;
}

return 0;
