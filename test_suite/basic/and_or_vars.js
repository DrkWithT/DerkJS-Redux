/* test logical operators of AND, OR */

var x = 10;
var y = 0;

var ans_2 = y || x * x;

/* ans_2 MUST be 100 */
console.log("PASS:", ans_2 === 100);
