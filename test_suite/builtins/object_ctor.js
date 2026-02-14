// Test Object ctor

var ok = 0;

if (!Object.prototype || !Object.prototype.constructor) {
    console.log("Bad Object.prototype / ctor:", Object.prototype, Object.prototype.constructor);
} else {
    ok = ok + 1;
}

var foo = Object({x: 1});

if (!foo.x) {
    console.log("foo is not an Object with 'x'.");
} else {
    ok = ok + 1;
}

console.log("PASS:", ok === 2);
