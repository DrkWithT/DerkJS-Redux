/* Test more array methods. */

var ok = 0;
var junk = [null, 69, 420];

if (junk.indexOf(69, 0) === 1) {
    ++ok;
} else {
    console.log("Failed junk.indexOf(69).");
}

/* 2 */
if (junk.lastIndexOf(69, -1) === 1) {
    ++ok;
} else {
    console.log("Failed junk.lastIndexOf(69).");
}

var junk2 = junk.reverse();
var junk2Str = junk2.join();

if (junk2Str === "420,69,") {
    ++ok;
} else {
    console.log("Failed junk.reverse():", junk2Str);
}

if (ok === 3) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
