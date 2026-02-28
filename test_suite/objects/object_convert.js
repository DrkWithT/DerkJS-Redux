/// test Object(value) conversions with primitives

var num = new Object(69);
var ok = 0;

if (typeof num === 'object') {
    ++ok;
} else {
    console.log("num is not a boxed Number.");
}

if (num.valueOf() === 69.0) {
    ++ok;
} else {
    console.log("num.valueOf() is not the funny number:", num.valueOf());
}

var foo = Object({x: 1});

if (foo.x === 1) {
    ++ok;
} else {
    console.log("var foo is not a JS object with 'x'.");
}

var bar = Object(true);

if (bar.valueOf() === true) {
    ++ok;
} else {
    console.log("var bar is not a JS Boolean object.");
}

if (ok === 4) {
    console.log("PASS");
} else {
    throw new Error("Failed case, see logs.");
}
