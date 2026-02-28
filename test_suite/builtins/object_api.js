/**
 * @file Test the basic `Object` interface:
 * Methods:
 * Object.constructor(arg)
 * Object.create(proto_arg)
 * Object.freeze()
 */

var ok = 0;
var foo = {
    secret: 69
};

var test = Object.create(foo);

if (test.secret === 69) {
    ok = ok + 1;
} else {
    /// NOTE: exit with status 1 if the result isn't the funny number.
    console.log("Unexpected test.secret:", test.secret);
}

Object.freeze(test);
test.secret = null;

if (test.secret === 69) {
    ok = ok + 1;
} else {
    console.log("Object.freeze failed on 'test', test.secret:", test.secret);
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
