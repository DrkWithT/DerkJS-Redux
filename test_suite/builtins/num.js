// test Number methods:

var x = new Number(10.7525);
var ok = 0;

var xAsPrimitive = x.valueOf();

if (xAsPrimitive === 10.7525) {
    ++ok;
} else {
    console.log("FAIL: unexpected x.valueOf():", xAsPrimitive);
}

var xToHundredth = x.toFixed(2);

if (xToHundredth === "10.75") {
    ++ok;
} else {
    console.log("FAIL: unexpected xToHundredth:", xToHundredth);
}

console.log("PASS:", ok === 2);
