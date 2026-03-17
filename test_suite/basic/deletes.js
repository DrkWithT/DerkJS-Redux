'use strict';

var foo = {
    x: 1
};

Object.freeze(foo);

try {
    delete foo.x;
} catch (err) {
    // this should catch a TypeError message!
    console.log(err.toString());
}
