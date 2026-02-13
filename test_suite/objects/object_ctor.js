// Test Object ctor

if (!Object.prototype || !Object.prototype.constructor) {
    console.log("Bad Object.prototype / ctor:", Object.prototype, Object.prototype.constructor);
    return 1;
}

var foo = Object({x: 1});

if (!foo.x) {
    console.log("foo is not an Object with 'x'.");
    return 1;
}

// TODO: add boxing of primitives as objects: Boolean, Number should have constructors and valueOf.

return 0;
