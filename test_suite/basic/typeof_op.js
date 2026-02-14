var checks = 0;

if (typeof undefined === "undefined") {
    checks = checks + 1;
} else {
    console.log("typeof undefined isn't 'undefined'");
}

if (typeof 1 === "number") {
    checks = checks + 1;
} else {
    console.log("typeof 1 isn't 'number'");
}

if (typeof 2.5 === "number") {
    checks = checks + 1;
} else {
    console.log("typeof 2.5 isn't 'number'");
}

if (typeof null === "object") {
    checks = checks + 1;
} else {
    console.log("typeof null isn't 'object'");
}

if (typeof [] === "object") {
    checks = checks + 1;
} else {
    console.log("typeof [] isn't 'object'");
}

if (typeof {} === "object") {
    checks = checks + 1;
} else {
    console.log("typeof {} isn't 'object'");
}

if (typeof function() { return undefined; } === "function") {
    checks = checks + 1;
} else {
    console.log("typeof (lambda) isn't 'function'");
}

console.log("PASS:", checks === 7);
