// Test more complex array methods

var ok = 0;
var test = [1, 2, 3];

var testStr1 = test.join();

if (testStr1 == "1,2,3") {
    ok = ok + 1;
} else {
    console.log("default delimited test.join() call failed.");
}

var testStr2 = test.join(' ');

if (testStr2 == "1 2 3") {
    ok = ok + 1;
} else {
    console.log("spaced test.join() call failed.");
}

var test2 = test.concat(4, [3, 2], 1);
var testStrTest2 = test2.join();

if (testStrTest2 === "1,2,3,4,3,2,1") {
    ok = ok + 1;
} else {
    console.log("unexpected test2 repr:", testStrTest2);
}

console.log("PASS:", ok === 3);
