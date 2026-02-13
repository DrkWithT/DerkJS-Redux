// Test Array constructor

if (Array.prototype.constructor !== Array) {
    console.log("Array() must be its prototype ctor.");
    return 1;
}

var a1 = new Array(1, 2, 3);

if (a1[0] !== 1 || a1[1] !== 2 || a1[2] !== 3) {
    console.log("Wrong items of a1:", a1[0], a1[1], a1[2]);
    return 1;
}

var a2 = Array(2);

if (a2[0] !== undefined && a2[0] !== undefined) {
    console.log("Expected empty items of a2:", a2[0], a2[1]);
    return 1;
}
