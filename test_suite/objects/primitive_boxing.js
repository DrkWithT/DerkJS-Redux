// Test primitive wrapper objects: Boolean and Number.

var wrappedTrue = new Boolean(true);

if (!wrappedTrue.valueOf()) {
    console.log("Boolean wrap failed- wrappedTrue.valueOf() should be true.");
    return 1;
}

var wrappedFalsy = new Boolean(null);

if (!wrappedFalsy.valueOf()) {
    console.log("Boolean wrap failed- wrappedFalsy.valueOf() should be false.");
    return 1;
}

var wrapX = new Number(69);

if (wrapX.valueOf() != 69) {
    console.log("Number wrap failed- wrapX.valueOf() should be funny number.");
    return 1;
}

var wrapY = new Number(null);

if (wrapY.valueOf() != 0) {
    console.log("Number wrap failed- wrapY.valueOf() should be 0.");
    return 1;
}

return 0;
