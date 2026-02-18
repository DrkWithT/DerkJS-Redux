// Test string prototype ctor

var ok = 0;

if (String.prototype.constructor === String) {
    ok = ok + 1;
} else { 
    console.log("String.prototype.constructor invalid:", String.prototype.constructor);
}

// The String ctor argument must be stringified.
var testStr = new String("1234");

// string comparison fails because testStr is still a number... bytecode gen fails to differ string literals properly, so the frontend should keep quotes for uniqueness from `1234` vs. `"1234"` but the actual strings must discard quotes.
if (testStr === "1234") {
    ok = ok + 1;
} else {
    console.log("String ctor failed, invalid testStr:", testStr);
}

console.log("PASS:", ok === 2);
