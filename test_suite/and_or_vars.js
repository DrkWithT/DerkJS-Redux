/* test logical operators of AND, OR */

var a = 10;
var b = 0;
var c = null;

/* ans_1 must be 10 */
var ans_1 = b || c || a;

/* ans_2 must be 100 */
var ans_2 = ans_1 && a * a;

return ans_2;
