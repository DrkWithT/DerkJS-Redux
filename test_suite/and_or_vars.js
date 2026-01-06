/* test logical operators of AND, OR */

var x = 10;
var y = 0;
var z = null;

var ans_2 = z || y || x * x;

/* ans_2 MUST be 100 */
return ans_2;
