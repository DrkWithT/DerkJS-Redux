// Test Array.prototype.forEach():

var info = {
    calls: 0,
    total: 0
};

function updateChecks(arg) {
    ++this.calls;
    this.total = this.total + arg;

    console.log("Call:", this.calls, "Total:", this.total);
    return undefined;
}

var test = [1, 2, 3, 4, 5, 6];
var discard = test.forEach(updateChecks, info);

console.log("PASS:", info.calls === 6, info.total === 21);
