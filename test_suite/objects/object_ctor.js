// Test Object ctor

if (Object.prototype.constructor !== Object) {
    console.log("Object() must be its prototype ctor.");
    return 1;
}

var foo = Object({x: 1});

if (!foo.x) {
    console.log("foo is not an Object with 'x'.");
    return 1;
}

// TODO: add boxing of primitives as objects: Boolean, Number should have constructors and valueOf.

return 0;
