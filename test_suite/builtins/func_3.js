// Test function length:

function foo() {
    return 1;
}

function bar(a, b, c) {
    return a + b + c;
}

var ok = 0;

if (foo.length === 0) {
    ++ok;
} else {
    console.log("foo.length is not 0.");
}

if (bar.length === 3) {
    ++ok;
} else {
    console.log("bar.length is not 3.");
}

if (ok === 2) {   
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
