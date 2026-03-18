// TEST Function():

var ok = 0;

var dudFn = Function();
var answer0 = dudFn();

if (dudFn.toString() === Function.prototype.toString()) {
    ++ok;
} else {
    console.log("dudFn is not Function.prototype: ", dudFn, Function.prototype);
}

var getAnswer = new Function("return 42;");
var answer1 = getAnswer();

if (answer1 === 42) {
    ++ok;
} else {
    console.log("answer1 != 42: ", answer1);
}

var addTwo = new Function("a", "b", "return a + b;");
var answer2 = addTwo(10, 20);

if (answer2 === 30) {
    ++ok;
} else {
    console.log("answer2 != 30: ", answer2);
}

if (ok === 3) {   
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
