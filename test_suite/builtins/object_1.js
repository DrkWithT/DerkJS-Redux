/*
 Test basic property accesses:
 Get or set properties.
 */

var num_1 = {
    x: 3
};

num_1.x = 2;

/* Should be 5 */
console.log("PASS:", num_1.x + num_1.x + 1 === 5);
