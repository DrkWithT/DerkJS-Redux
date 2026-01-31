/**
 * @file str_to_ints.js
 * @summary Test parseInt(string s). I don't have the radix argument yet.
 * @author DrkWithT
 */

var a = parseInt("foo");
var b = parseInt("15");
var c = parseInt("69");
var d = parseInt("420");

console.log(a + b); // NaN
console.log(c + d); // 435

return 0;
