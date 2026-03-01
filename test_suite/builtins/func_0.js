// test Function object basics (not methods):

var ok = 0;

if (typeof Function.prototype === "function") {
    ++ok;
} else {
    console.log("Function.prototype must be a function.");
}

if (Function.prototype.prototype === undefined) {
    ++ok;
} else {
    console.log("Function.prototype.prototype is not undefined.");
}

if (Object.prototype.isPrototypeOf(Function.prototype)) {
    ++ok;
} else {
    console.log("Function.prototype.__proto__ is not Object.prototype.");
}

if (ok === 3) {
    console.log("PASS");
} else {
    throw new Error("Failed test, see logs.");
}
