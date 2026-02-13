/**
 * @file Test the basic `Object` interface:
 * Methods:
 * Object.constructor(arg)
 * Object.create(proto_arg)
 * Object.freeze()
 */

var foo = {
    secret: 69
};

var test = Object.create(foo);

if (test.secret != 69) {
    /// NOTE: exit with status 1 if the result isn't the funny number.
    console.log("Unexpected test_child.secret:", test.secret);
    return 1;
}

Object.freeze(test);
test.secret = null;

if (test.secret != 69) {
    console.log("Object.freeze failed on 'test', test.secret:", test.secret);
    return 1;
}

return 0;
