// test failure of assignments to frozen object

var ok = true;
console.readln = null;

if (!console.readln) {
    ok = false;
}

console.log("PASS:", ok);
