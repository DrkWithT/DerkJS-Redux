// Test native isNaN:

var tests = [undefined, null, true, false, 42, 0 / 0];
var expected = [true, false, false, false, false, true];
var resultsNaN = tests.map(
    function (item) {
        return isNaN(item);
    },
    undefined
);

console.log("PASS:", resultsNaN === expected, "resultsNaN:", resultsNaN);
