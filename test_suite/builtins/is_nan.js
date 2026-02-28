// Test native isNaN:

var tests = [undefined, null, true, false, 42, 0 / 0, {}, []];
var expected = [true, false, false, false, false, true, true, false];
var resultsNaN = tests.map(
    function (item) {
        return isNaN(item);
    },
    this
);

var resultsStr = resultsNaN.join();
var expectedStr = expected.join();

if (resultsStr === expectedStr) {
    console.log("PASS");
} else {
    console.log("Bad match:", resultsStr, " != ", expectedStr);
    throw new Error("Test failed, see logs.");
}
