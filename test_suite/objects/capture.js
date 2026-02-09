var outside = 10;

function test() {
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

// Should yield 10 & 42
console.log("test:", test());
console.log("test2:", test2());

return 0;
