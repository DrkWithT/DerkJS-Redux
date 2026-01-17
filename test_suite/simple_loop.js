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

/* Should yield 55 */
return consecutive_sum(10);
