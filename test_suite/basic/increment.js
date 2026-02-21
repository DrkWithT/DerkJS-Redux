// test prefix ++ and -- operators

var ok = 0;

var test1 = undefined;
var test2 = 1.5;
var test3 = {
    secret: 68
};

++test1;

if (String(test1) === "NaN") {
    ++ok;
} else {
    console.log("++test1 failed:", test1);
}

++test2;

if (test2 === 2.5) {
    ++ok;
} else {
    console.log("++test2 failed:", test2);
}

++test3.secret;

if (test3.secret === 69) {
    ++ok;
} else {
    console.log("++test3 failed:", test3);
}

console.log("PASS:", ok === 3);
