/* FizzBuzz & try native clock object */

function fizzbuzz(n) {
    var count = 1;

    while (count <= n) {
        if (count % 3 == 0 && count % 5 == 0) {
            console.log("FizzBuzz");
        } else if (count % 5 == 0) {
            console.log("Buzz");
        } else if (count % 3 == 0) {
            console.log("Fizz");
        } else {
            console.log(count);
        }

        count = count + 1;
    }

    return true;
}

var discard = fizzbuzz(30);

return 0;
