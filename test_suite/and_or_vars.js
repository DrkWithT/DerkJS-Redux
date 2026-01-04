/* test logical operators of AND, OR */

var a = 10;
var b = 0;
var c = null;

/* ans_1 must be 10 */
var ans_1 = c || a;

/* ans_2 must be 10 */
var ans_2 = b || c || a;

/* ans_3 must be 20 */
var ans_3 = ans_1 && ans_1 + ans_2;

return ans_3;
