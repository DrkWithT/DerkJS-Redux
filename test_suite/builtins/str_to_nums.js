/**
 * @file str_to_nums.js
 * @summary Test parseInt(s, radix?) and parseFloat(s).
 * @author DrkWithT
 */

var c = parseInt("69");
var d = parseInt("420");
var x = parseFloat("2.5");
var y = parseFloat("abc");

console.log("PASS 1:", c + d === 489);
console.log("PASS 2:", x === 2.5 && isNaN(y));
