// Test Error throws & try-catch

try {   
    throw new Error("TESTING!");
} catch (err) {
    console.log("Caught:");
    console.log(err);
}
