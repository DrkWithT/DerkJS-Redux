/* test logical operators of AND, OR */

var x = 10;
var y = 0;
var z;

var ans_1 = z === undefined;
var ans_2 = y || x * x;
var ok = 0;

if (ans_1) {
    ++ok;
} else {
    console.log("'var z;' is not undefined.");
}

if (ans_2 === 100) {
    ++ok;
} else {
    console.log("'ans_2' is incorrect:", ans_2, "vs.", 100);
}

/* ans_2 MUST be 100 */
if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
