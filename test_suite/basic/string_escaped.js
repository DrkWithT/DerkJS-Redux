// test escaped strings

var ok = 0;

var paddedWord = "\n\ttest\n\n";
var unpaddedWord = paddedWord.trim();

if (unpaddedWord === "test") {
    ++ok;
} else {
    console.log("Trim of escaped whitespace failed: unpaddedWord: ", unpaddedWord);
}

var msg = "\x0A\n";

if (msg.charCodeAt(0) === 10 && msg.charCodeAt(1) === 10) {
    ++ok;
} else {
    console.log("Encode of ASCII 0A (LF) failed for msg: ", msg.charCodeAt(0), msg.charCodeAt(1));
}

if (ok === 2) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
