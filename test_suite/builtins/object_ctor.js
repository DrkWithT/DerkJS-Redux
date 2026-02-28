// Test Object ctor

var ok = 0;

if (!Object.prototype || !Object.prototype.constructor) {
    console.log("Bad Object.prototype / ctor:", Object.prototype, Object.prototype.constructor);
} else {
    ++ok;
}

var foo = Object({x: 1});

if (!foo.x) {
    console.log("foo is not an Object with 'x'.");
} else {
    ++ok;
}

if (ok === 2) {   
    console.log("PASS:");
} else {
    throw new Error("Test failed, see logs.");
}
