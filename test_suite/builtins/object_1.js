/*
 Test basic property accesses:
 Get or set properties.
 */

var num_1 = {
    x: 3
};

num_1.x = 2;

if (num_1.x === 2) {
    console.log("PASS");
} else {
    console.log("num_1.x != 2:", num_1.x);
    throw new Error("Test failed, see logs.");
}
