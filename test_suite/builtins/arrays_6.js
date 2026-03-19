// Test Array.prototype.map and Array.prototype.filter:

var ok = 0;
var brainrot = [69, false, 420, null, 67, 15, true];
var counts = [1, 2, 3];

var noBrainrot = brainrot.filter(function (arg, index, items) { return typeof arg !== "number"; });
var noBrainrotStr = noBrainrot.join();

if (noBrainrotStr === "false,,true") {
    ++ok;
} else {
    console.log("Wrong brainrot.filter result:", noBrainrotStr);
}

var squaresTo3 = counts.map(function (arg, index, items) {
    return arg * arg;
});
var squaresTo3Str = squaresTo3.join(); 

if (squaresTo3Str === "1,4,9") {
    ++ok;
} else {
    console.log("Wrong counts.map(<square fn>) result:", squaresTo3Str);
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
