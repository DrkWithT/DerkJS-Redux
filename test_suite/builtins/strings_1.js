/**
 * @file strings_1.js
 * @summary Test string methods.
 */

var ok = 0;
var a = "ones";

if (a.charCodeAt(0) == 111) {
    ok = ok + 1;
} else {
    console.log("Unexpected a[0] char code.");
}

if (a.len() === 4) {
    ok = ok + 1;
} else {
    console.log("Unexpected a.len(): not 4.");
}

if (a.substr(0, 3) === "one") {
    ok = ok + 1;
} else {
    console.log("Unexpected a.substr(0, 3): not 'one'.");
}

console.log("PASS:", ok === 3);
