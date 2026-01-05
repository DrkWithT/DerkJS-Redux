/* test while loops with gaussian sum */

function consecutive_sum(n) {
    var count = n || 0;
    var sum = count + count;

    /* TODO: add djs_replace instruction for mutable assignments on variables! */
    /*
    while (count > 0) {
        sum = sum + count;
        count = count - 1;
    }
    */

    return sum;
}

/* Should yield 3 */
return consecutive_sum(2);
