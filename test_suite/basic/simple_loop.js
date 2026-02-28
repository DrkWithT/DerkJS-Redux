/* test while loops with gaussian sum */

function consecutive_sum(n) {
    var count = n;
    var sum = 0;

    while (count > 0) {
        sum = sum + count;
        count = count - 1;
    }

    return sum;
}

var ans = consecutive_sum(10);

if (ans === 55) {
    console.log("PASS");
} else {
    console.log("Results:", ans);
    throw new Error("Test failed, see logs.");
}
