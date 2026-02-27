// TEST Function():

var getAnswer = new Function("return 42;");

var answer1 = getAnswer();

var addTwo = new Function("a", "b", "return a + b;");

var answer2 = addTwo(10, 20);

console.log("PASS:", answer1 === 42, answer2 === 30);
