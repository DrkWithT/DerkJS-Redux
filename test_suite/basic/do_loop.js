// test do-while loops:

var ok = 0;

var a = 0;

do {
    a = a + 1;
} while (a < 10);

if (a === 10) {
    ok++;
} else {
    console.log("simple do-while failed: a != 10: ", a);
}

var b = 0;

do {
    b = b + 1;

    if (b === 5) {
        break;
    }
} while (true);

if (b === 5) {
    ok++;
} else {
    console.log("break in do-while failed: b != 5: ", b);
}

var c = 10;

do {
    c = c - 1;

    if (c > 0) {
        continue;
    }
} while (false);

if (c === 0) {
    ok++;
} else {
    console.log("break in do-while failed: c != 0: ", c);
}

if (ok === 3) {
    console.log("PASS");
} else {
    throw new Error("Test failed, see logs.");
}
