function make_iota(n) {
    var x = n;

    function iota() {
        x += 1;
        return x;
    }

    return iota;
}

var counter = make_iota(1);

display(counter());
display(counter());
display(counter());
