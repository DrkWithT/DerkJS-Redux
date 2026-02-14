// Test Array constructor

var ok = 0;

if (Array.prototype.constructor === Array) {
    ok = ok + 1;
} else {
    console.log("Array() must be its prototype ctor.");
}

var a1 = new Array(1, 2, 3);

if (a1[0] === 1 || a1[1] === 2 || a1[2] === 3) {
    ok = ok + 1;
} else {
    console.log("Wrong items of a1:", a1[0], a1[1], a1[2]);
}

var a2 = Array(2);

if (a2[0] === undefined && a2[1] === undefined) {
    ok = ok + 1;
} else {
    console.log("Expected empty items of a2:", a2[0], a2[1]);
}

console.log("PASS:", ok === 3);
