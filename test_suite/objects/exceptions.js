// Test Error throws & try-catch

var ok = 0;
var blankErr = Error(undefined);

try {
    throw new Error("TESTING!");
} catch (err) {
    console.log("Caught, OK:  ", err.toString());
    ++ok;
} finally {
    ++ok;
    console.log("Finally reached, OK");
}

if (blankErr.message === '') {
    ++ok;
} else {
    console.log("Unexpected blankErr.message, blank: ", blankErr.message);
}

if (ok === 3) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
