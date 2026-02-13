/// Test a slightly modified example case from zoo.js.org which Flathead would fail... Except that top-level returns as exits are kinda cool.

function Obj() { this.x = 1; return this; }

function f() { return this.x; }

var o = new Obj();
o.f = f;

if (o.f() != 1) {
    console.log("Invalid o.f() result:", o.f());
    return 1;
}

return 0;
