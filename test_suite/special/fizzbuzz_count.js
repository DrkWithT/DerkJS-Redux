function count_fizz_or_buzz(n) {
    var next_mult_3 = 0, next_mult_5 = 0, next_n = 0, answer = 0;

    while (next_n <= n) {
        if (next_n == next_mult_3 || next_n == next_mult_5) { answer = answer + 1; }

        if (next_n == next_mult_3) { next_mult_3 = next_mult_3 + 3; }

        if (next_n == next_mult_5) { next_mult_5 = next_mult_5 + 5; }

        next_n = next_n + 1;
    }

    return answer;
}

var ans = count_fizz_or_buzz(10);

/*
 FizzBuzz: (0) 1 2 (3) 4 (5) (6) 7 8 (9) (10)
 Count: 6
 */
console.log("PASS:", count_fizz_or_buzz(10) === 6);

return 0;
