var outside = 10;

function test() {
    outside = 42;
    console.log(outside);

    return outside;
}

function test2() {
    var outside = 20;
    var test3 = function() {
        return outside * 2 + 2;
    };

    return test3();
}

var ans1 = test();
var ans2 = test2();
var ok = 0;

if (ans1 === 42) {
    ++ok;
} else {
    console.log("wrong ans1 value:", ans1);
}

if (ans2 === 42) {
    ++ok;
} else {
    console.log("wrong ans2 value:", ans2);
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
