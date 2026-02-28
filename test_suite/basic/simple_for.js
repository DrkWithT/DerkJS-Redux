function sumMultiplesOf(num, count) {
    var sum = 0;

    for (var i = 0; i <= count; ++i) {
        sum = sum + num * i;
        console.log("Multiple #", i, "-> sum =", sum);
    }

    return sum;
}

var ans = sumMultiplesOf(2, 10);

if (ans === 110) {
    console.log("PASS");
} else {
    console.log("Results:", ans);
    throw new Error("Test failed, see logs.");
}
