// Test Array.prototype.forEach():

var info = {
    count: 0,
    sum: 0
};

function updateChecks(arg) {
    this.count = this.count + 1;
    this.sum = this.sum + arg;

    return undefined;
};

var test = [1, 2, 3, 4];

test.forEach(updateChecks, info);

if (info.count !== 4 || info.sum !== 10) {
    console.log("test.forEach(updateChecks, info) failed:", info.count, info.sum);
    return 1;
}

return 0;
