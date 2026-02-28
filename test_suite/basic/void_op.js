function test() {
    return 123;
}

var discarded = void test();
var junk = void(0);
var ok = 0;

if (discarded === undefined) {
    ++ok;
} else {
    console.log("void test() is not undefined.");
}

if (junk === undefined) {
    ++ok;
} else {
    console.log("void(0) is not undefined.");
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
