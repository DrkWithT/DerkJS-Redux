var secret1 = 21;

if (secret1 > 0) {
    var secret2 = 2;
} else {
    var secret2 = -2;
}

// Must be `true`
console.log("PASS:", secret1 * secret2 === 42);
