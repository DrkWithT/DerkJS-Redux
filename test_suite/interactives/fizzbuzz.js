/* FizzBuzz & try native clock object */

function fizzbuzz(n) {
    var fizzBuzzes = 0;
    var count = 1;

    while (count <= n) {
        if (count % 3 == 0 && count % 5 == 0) {
            console.log("FizzBuzz");
            fizzBuzzes = fizzBuzzes + 1;
        } else if (count % 5 == 0) {
            console.log("Buzz");
        } else if (count % 3 == 0) {
            console.log("Fizz");
        } else {
            console.log(count);
        }

        count = count + 1;
    }

    return fizzBuzzes;
}

var ans = fizzbuzz(30);

console.log("PASS:", ans === 3);
