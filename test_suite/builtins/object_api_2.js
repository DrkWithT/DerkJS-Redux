// Test more Object.[[prototype]] methods...

var testProto = {
    secret: 69
};
var testObject = Object.create(testProto);
var ok = 0;

testObject.data = 42;

if (testProto.isPrototypeOf(testObject)) {
    ++ok;
} else {
    console.log("FAIL: testObject.__proto__ is not testProto.");
}

if (testObject.hasOwnProperty("data")) {
    ++ok;
} else {
    console.log("FAIL: testObject.data is missing.");
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
