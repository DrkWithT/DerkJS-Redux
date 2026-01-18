/* FizzBuzz & try native clock object */

function fizzbuzz(n) {
    var count = 1;

    while (count <= n) {
        if (count % 3 && count % 5) {
            console.log("FizzBuzz");
        } else if (count % 5) {
            console.log("Buzz");
        } else if (count % 3) {
            console.log("Fizz");
        } else {
            console.log(count);
        }

        count = count + 1;
    }

    return true;
}

var fizz_start_ms = clock.now();
var discard = fizzbuzz(100);
var fizz_end_ms = clock.now();

console.log("FizzBuzz 100 (ms): ", fizz_end_ms - fizz_start_ms);

return 0;
