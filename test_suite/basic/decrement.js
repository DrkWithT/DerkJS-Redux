// test prefix -- operator

var ok = 0;

var a = null;
var b = 43;
var c = {
    secret: 70
};
var d = 0;

--a;

if (a === -1) {
    ++ok;
} else {
    console.log("--a failed:", a);
}

--b;

if (b === 42) {
    ++ok;
} else {
    console.log("--b failed:", b);
}

--c.secret;

if (c.secret === 69) {
    ++ok;
} else {
    console.log("--c.secret failed:", c.secret);
}

if (d-- === 0) {
    console.log("after d--:", d);
    ++ok;
} else {
    console.log("Failed d--.");
}

if (ok === 4) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
