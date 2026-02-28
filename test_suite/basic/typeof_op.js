var ok = 0;

if (typeof undefined === "undefined") {
    ++ok;
} else {
    console.log("typeof undefined isn't 'undefined'");
}

if (typeof 1 === "number") {
    ++ok;
} else {
    console.log("typeof 1 isn't 'number'");
}

if (typeof 2.5 === "number") {
    ++ok;
} else {
    console.log("typeof 2.5 isn't 'number'");
}

if (typeof null === "object") {
    ++ok;
} else {
    console.log("typeof null isn't 'object'");
}

if (typeof [] === "object") {
    ++ok;
} else {
    console.log("typeof [] isn't 'object'");
}

if (typeof {} === "object") {
    ++ok;
} else {
    console.log("typeof {} isn't 'object'");
}

if (typeof function() { return undefined; } === "function") {
    ++ok;
} else {
    console.log("typeof (lambda) isn't 'function'");
}

if (ok === 7) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
