// test prefix ++ and -- operators

var ok = 0;

var test1 = undefined;
var test2 = null;
var test3 = 0;
var test4 = 1.5;
var test5 = {};
var test6 = {
    secret: 68
};

++test1;

if (test1 === NaN) {
    ++ok;
} else {
    console.log("++test1 failed:", test1);
}

++test2;

if (test2 === 1) {
    ++ok;
} else {
    console.log("++test2 failed:", test1);
}

++test3;

if (test3 === 1) {
    ++ok;
} else {
    console.log("++test3 failed:", test1);
}

++test4;

if (test4 === 2.5) {
    ++ok;
} else {
    console.log("++test4 failed:", test1);
}

++test5;

if (test5 === NaN) {
    ++ok;
} else {
    console.log("++test5 failed:", test5);
}

++test6.secret;

if (test6.secret === 69) {
    ++ok;
} else {
    console.log("++test6 failed:", test6);
}

console.log("PASS:", ok === 6);
