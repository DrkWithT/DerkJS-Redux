var items = [
    1,
    2,
    3,
    true
];

var first = items.at(0);
var last = items.pop();
var postN = items.push(2, 1);
var ok = 0;

if (first === 1) {
    ++ok;
} else {
    console.log("Wrong first, expected 1: ", first);
}

if (last) {
    ++ok;
} else {
    console.log("Wrong last, expected true: ", last);
}

if (postN === 5) {
    ++ok;
} else {
    console.log("Wrong postN, expected 5: ", postN);
}

if (ok === 3) {
    console.log("PASS");
} else {
    throw new Error("Failed test, see logs.");
}
