var outside = 10;

function test() {
    console.log(outside);
    return outside;
}

// Should yield 20
console.log("test:", test());

return 0;
