// test failure of assignments to frozen object

console.log = null;

if (!console.log) {
    return 1;
}

return 0;
