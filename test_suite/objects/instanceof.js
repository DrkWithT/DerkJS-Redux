// test instanceof operator:

function Pair(a, b) {
    this.a = a;
    this.b = b;
}

var foo = {x: 1};
var bar = new Pair(10, 20);

var ok = 0;

if (foo instanceof Object) {
    ++ok;
} else {
    console.log("foo lacks Object.prototype in its __proto__.");
}

if (bar instanceof Pair) {
    ++ok;
} else {
    console.log("bar lacks Pair.prototype in its __proto__.", Pair.prototype.isPrototypeOf(bar));
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
