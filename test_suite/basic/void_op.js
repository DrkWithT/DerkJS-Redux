function test() {
    return 123;
}

if (void test() !== undefined) {
    console.log("void discard operator failed.");
    return 1;
}

return 0;
