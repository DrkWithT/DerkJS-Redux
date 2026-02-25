// TEST Function():

var addTwo = new Function("a", "b", "return a + b;");

var answer = addTwo(10, 20);

console.log("PASS:", answer === 30);
