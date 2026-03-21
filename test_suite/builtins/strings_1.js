/**
 * @file strings_1.js
 * @summary Test string methods.
 */

var ok = 0;
var a = "ones";

if (a.charCodeAt(0) == 111) {
    ++ok;
} else {
    console.log("Unexpected a[0] char code.");
}

if (a.charAt(0) === 'o') {
    ++ok;
} else {
    console.log("a.charAt() failed: result != 'o'");
}

if (a.charAt(10) === '') {
    ++ok;
} else {
    console.log("OOB a.charAt() failed: result != ''");
}

if (a.length === 4) {
    ++ok;
} else {
    console.log("Unexpected a.length, not 4:", a.length);
}

if (a.substr(0, 3) === "one") {
    ++ok;
} else {
    console.log("Unexpected a.substr(0, 3): not 'one'.");
}

if (ok === 5) {   
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}