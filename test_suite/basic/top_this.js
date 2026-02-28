this.x = 10;

if (this.x === 10) {
    console.log("PASS");
} else {
    console.log("Results:", this.x);
    throw new Error("Test failed, see logs.");
}
