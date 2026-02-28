/* Test undefined, NaN results by invalid math... */

var a = 10;
var b = undefined;
var ans = a + b;

/* This should yield NaN. */
if (isNaN(ans)) {
    console.log("PASS");
} else {
    console.log("Results:", ans);
    throw new Error("Test failed, see logs.");
}
