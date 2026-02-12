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
var check = test.secret;

if (check != 69) {
    /// NOTE: exit with status 1 if the result isn't the funny number.
    console.log("Unexpected test_child.secret:", check);
    return 1;
}

return 0;
