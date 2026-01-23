this["Foo"] = 21;

var twiceOfFoo = function() {
    return this["Foo"] * 2;
};

console.log(twiceOfFoo());

return 0;
