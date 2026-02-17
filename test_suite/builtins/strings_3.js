// Test String.prototype.substring() AND String.prototype.trim():

var ok = 0;
var test = "Hello World";
var extraTest = "  foo bar  ";

if (!String.prototype.substring || !String.prototype.trim) {
    console.log("Missing String.prototype.substring, trim:", String.prototype.substring, String.prototype.trim);
} else {
    ok = ok + 1;
}

// Maybe I should note the edge case of a not-passed end index.
var testedSubstr = test.substring(6, 11);

if (testedSubstr === "World") {
    ok = ok + 1;
} else {
    console.log("Unexpected testedSubstr, expected 'World':", testedSubstr);
}

var testedSubstr2 = test.substring(1, 5);

if (testedSubstr2 === "ello") {
    ok = ok + 1;
} else {
    console.log("Unexpected testedSubstr2, expected 'ello':", testedSubstr2);
}

var trimmedExtra = extraTest.trim();

if (trimmedExtra === "foo bar") {
    ok = ok + 1;
} else {
    console.log("Unexpected trimmedExtra, expected 'foo bar':", trimmedExtra);
}

console.log("PASS:", ok === 4);
