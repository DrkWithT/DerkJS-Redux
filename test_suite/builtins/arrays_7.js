// test array.length accessor:

var ok = 0;
var foo = [1, 2, 3];

if (foo.length === 3) {
    ok = ok + 1;
} else {
    console.log("Unexpected foo.length:", foo.length);
}

foo.length = 1;

if (foo.length === 1) {
    ok = ok + 1;
} else {
    console.log("Unexpected foo.length:". foo.length);
}

if (foo.at(100) === undefined) {
    ok = ok + 1;
} else {
    console.log("Unexpected foo[100], should be undefined:", foo.at(100));
}

console.log("PASS:", ok === 3);
