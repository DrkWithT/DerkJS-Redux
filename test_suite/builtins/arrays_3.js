/* Test more array methods. */

var ok = 0;
var junk = [null, 69, 420];

if (junk.indexOf(69) === 1) {
    ok = ok + 1;
} else {
    console.log("Failed junk.indexOf(69).");
}

/* 2 */
if (junk.lastIndexOf(69) === 1) {
    ok = ok + 1;
} else {
    console.log("Failed junk.lastIndexOf(69).");
}

/* 420, 69, null */
var junk2 = junk.reverse();
var junk2Str = junk2.join();

if (junk2Str === "420,69,") {
    ok = ok + 1;
} else {
    console.log("Failed junk.reverse():", junk2Str);
}

console.log("PASS:", ok === 3);
