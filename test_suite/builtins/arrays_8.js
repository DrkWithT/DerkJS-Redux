// test Array.prototype.some, Array.prototype.every:

var ok = 0;

var items = [-4, 2, 0, 1, -4];
var extra = {
    x: 0
};

function isEnough(arg, index, items) {
    return arg >= this.x;
}

var check1 = items.some(isEnough, extra);

if (check1) {
    ++ok;
} else {
    console.log("items.some() failed, expect true: ", check1);
}

var check2 = items.every(isEnough, extra);

if (!check2) {
    ++ok;
} else {
    console.log("items.some() failed, expect false: ", check1);
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
