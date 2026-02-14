var ok = 0;

var a = 1;
// TEST 1:
// a = 2;

// TEST 2:
var
// this should skip
b = 68;

if (a == 1) {
    ok = ok + 1;
}

if (a + b == 69) {
    ok = ok + 1;
}

// TEST 3:
/* / * a = 0; * / */

if (a != 0) {
    ok = ok + 1;
}

console.log("PASS:", ok === 3);
