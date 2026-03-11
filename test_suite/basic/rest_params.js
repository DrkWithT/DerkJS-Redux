// test 'rest parameter' basics:

function printStuff(...args) {
    args.forEach(function (arg) {
        console.log(arg);
    });

    return args.length;
}

var argc = printStuff(undefined, null, true, 42, 0 / 0);

if (argc === 5) {
    console.log("PASS");
} else {
    console.log("Unexpected variadic argument count vs. 5: ", argc);
    throw new Error("Test failed, see logs.");
}
