// Verify cannot convert an encrypted collection to a capped collection

/**
 * @tags: [
 *  featureFlagFLE2,
 * ]
 */
(function() {
'use strict';

const isFLE2Enabled = TestData == undefined || TestData.setParameters.featureFlagFLE2;

if (!isFLE2Enabled) {
    return;
}

const dbTest = db.getSiblingDB('convert_encrypted_to_capped_db');

dbTest.basic.drop();

const sampleEncryptedFields = {
    "fields": [
        {
            "path": "firstName",
            "keyId": UUID("11d58b8a-0c6c-4d69-a0bd-70c6d9befae9"),
            "bsonType": "string",
            "queries": {"queryType": "equality"}
        },
    ]
};

assert.commandWorked(dbTest.createCollection("basic", {encryptedFields: sampleEncryptedFields}));

assert.commandFailedWithCode(dbTest.runCommand({convertToCapped: "basic", size: 100000}),
                             6367302,
                             "Convert encrypted collection to capped passed");

assert.commandFailedWithCode(
    dbTest.runCommand({cloneCollectionAsCapped: "basic", toCollection: "capped", size: 100000}),
    6367302,
    "Clone encrypted collection as capped passed");
}());
