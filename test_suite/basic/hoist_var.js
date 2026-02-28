var secret1 = 21;

if (secret1 > 0) {
    var secret2 = 2;
} else {
    var secret2 = -2;
}

// Must be `true`
if (secret1 * secret2 === 42) {
    console.log("PASS");
} else {
    console.log("Results:", secret1, secret2);
    throw new Error("Test failed, see logs.");
}
