// TEST Function():

var getAnswer = new Function("return 42;");

var answer1 = getAnswer();

var addTwo = new Function("a", "b", "return a + b;");

var answer2 = addTwo(10, 20);

if (answer1 === 42 && answer2 === 30) {   
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
