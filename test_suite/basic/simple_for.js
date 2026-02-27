function sumMultiplesOf(num, count) {
    var sum = 0;

    for (var i = 0; i < count; ++i) {
        console.log("Multiple #", i, " -> sum =", sum);
        sum += num * i;
    }

    return sum;
}

var ans = sumMultiplesOf(2, 10);

console.log("PASS:", ans === 110, "ans =", ans);
