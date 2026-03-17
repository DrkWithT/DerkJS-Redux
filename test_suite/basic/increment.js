// test prefix ++ and -- operators

var ok = 0;

var test1 = undefined;
var test2 = 1.5;
var test3 = {
    secret: 68
};
var test4 = 0;

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

if (test4++ === 0) {
    console.log("after test4++:", test4);
    ++ok;
} else {
    console.log("Failed test4++.");
}

if (ok === 4) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
