/* Test more array methods. */

var junk = [null, undefined, 69, 420];

/* 2 */
console.log(junk.indexOf(69) === 2);

/* 2 */
console.log(junk.lastIndexOf(69) === 2);

/* 420, 69, undefined, null */
console.log(junk.reverse());

return 0;
