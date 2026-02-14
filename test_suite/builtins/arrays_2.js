var items = [
    1,
    2,
    3,
    true
];

var first = items.at(0);
var last = items.pop();
var postN = items.push(2, 1);

console.log(first == 1);
console.log(last == true);
console.log(postN == 5);
