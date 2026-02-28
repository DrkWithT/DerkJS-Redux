// Test primitive wrapper objects: Boolean and Number.

var ok = 0;
var wrappedTrue = new Boolean(true);

if (!wrappedTrue.valueOf()) {
    console.log("Boolean wrap failed- wrappedTrue.valueOf() should be true.");
} else {
    ++ok;
}

var wrappedFalsy = new Boolean(null);

if (wrappedFalsy.valueOf()) {
    console.log("Boolean wrap failed- wrappedFalsy.valueOf() should be false.");
} else {
    ++ok;
}

var wrapX = new Number(69);

if (wrapX.valueOf() != 69) {
    console.log("Number wrap failed- wrapX.valueOf() should be funny number.");
} else {
    ++ok;
}

var wrapY = new Number(null);

if (wrapY.valueOf() != 0) {
    console.log("Number wrap failed- wrapY.valueOf() should be 0:", wrapY.valueOf());
} else {
    ++ok;
}

if (ok === 4) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
