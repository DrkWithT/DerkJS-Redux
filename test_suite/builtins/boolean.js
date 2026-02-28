/// Test Boolean wrapper methods:

var good = new Boolean(42);
var bad = new Boolean(false);
var ok = 0;

if (good.valueOf()) {
    ++ok;
} else {
    console.log("FAIL: good != true", good.toString());
}

if (!bad.valueOf()) {
    ++ok;
} else {
    console.log("FAIL: bad != false", false.toString());
}

if (ok === 2) {   
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
