/// Test a slightly modified example case from zoo.js.org which Flathead would fail... Except that top-level returns as exits are kinda cool.

var ok = true;
function Obj() { this.x = 1; }

function f() { return this.x; }

var o = new Obj();
o.f = f;

if (o.f() != 1) {
    console.log("Invalid o.f(), expected 1:", o.f());
    throw new Error("Test failed, see logs.");
} else {
    console.log("PASS");
}
