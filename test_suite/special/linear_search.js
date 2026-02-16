/**
 * @file linear_search.js
 * @summary Test break & continue.
 */

function linearSearch(items, target) {
    var end = items.length;
    var pos = 0;
    var ans = -1;

    while (pos < end) {
        if (items.at(pos) == target) {
            ans = pos;
            break;
        }

        pos = pos + 1;
    }

    return ans;
}

var x = linearSearch([1, 3, 7, 6, 9, 4, 2, 0], 4);

console.log("PASS:", x === 5);
