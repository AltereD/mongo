/**
 * Test the $binaryPopCount expression.
 */
(function() {
"use strict";
load("jstests/aggregation/extras/utils.js");

const coll = db.expression_binaryPopCount;
coll.drop();

assert.commandWorked(coll.insert([
    {_id: 0, x: BinData(0, "")},
    {_id: 1, x: BinData(0, "//////////8=")}, // Represents -1
    {_id: 2, x: BinData(0, "/////////38=")}, // Represents int64 max => 9223372036854775807
    {_id: 3, x: BinData(0, "YWJj")}, // Represents abc => 3+3+4, 10 bits
    {_id: 4, x: BinData(0, "SGVsbG8=")}, // Represents Hello => 20 bits
    {_id: 5, x: null},
    {_id: 6},
]));


const result =
    coll.aggregate([{$sort: {_id: 1}}, {$addFields: {pc: {$binaryPopCount: "$x"}}}]).toArray();
assert.eq(result, [

    {_id: 0, x: BinData(0, ""), pc: null},
    {_id: 1, x: BinData(0, "//////////8="), pc: 64},
    {_id: 2, x: BinData(0, "/////////38="), pc: 63},
    {_id: 3, x: BinData(0, "YWJj"), pc: 10},
    {_id: 4, x: BinData(0, "SGVsbG8="), pc: 20},
    {_id: 5, x: null, pc: null},
    {_id: 6, pc: null},
]);


// $binaryPopCount only accepts BinData.
assert.commandWorked(coll.insert({x: 42}));
assertErrorCode(coll, {$project: {s: {$binaryPopCount: "$x"}}}, 56789);
}());
