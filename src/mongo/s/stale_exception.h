/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo/db/namespace_string.h"
#include "mongo/s/chunk_version.h"
#include "mongo/s/database_version.h"
#include "mongo/s/shard_id.h"
#include "mongo/util/concurrency/notification.h"

namespace mongo {

class StaleConfigInfo final : public ErrorExtraInfo {
public:
    static constexpr auto code = ErrorCodes::StaleConfig;

    StaleConfigInfo(NamespaceString nss,
                    ChunkVersion received,
                    boost::optional<ChunkVersion> wanted,
                    ShardId shardId,
                    boost::optional<SharedSemiFuture<void>> criticalSectionSignal = boost::none)
        : _nss(std::move(nss)),
          _received(received),
          _wanted(wanted),
          _shardId(shardId),
          _criticalSectionSignal(criticalSectionSignal) {}

    const auto& getNss() const {
        return _nss;
    }

    const auto& getVersionReceived() const {
        return _received;
    }

    const auto& getVersionWanted() const {
        return _wanted;
    }

    const auto& getShardId() const {
        return _shardId;
    }

    auto getCriticalSectionSignal() const {
        return _criticalSectionSignal;
    }

    void serialize(BSONObjBuilder* bob) const {
        bob->append("ns", _nss.ns());
        _received.appendLegacyWithField(bob, "vReceived");
        if (_wanted) {
            _wanted->appendLegacyWithField(bob, "vWanted");
        }

        invariant(_shardId != "");
        bob->append("shardId", _shardId.toString());
    }

    static std::shared_ptr<const ErrorExtraInfo> parse(const BSONObj& obj) {
        return std::make_shared<StaleConfigInfo>(parseFromCommandError(obj));
    }

    static StaleConfigInfo parseFromCommandError(const BSONObj& obj) {
        const auto shardId = obj["shardId"].String();
        invariant(shardId != "");

        auto extractOptionalChunkVersion =
            [&obj](StringData field) -> boost::optional<ChunkVersion> {
            try {
                return boost::make_optional<ChunkVersion>(
                    ChunkVersion::fromBSONLegacyOrNewerFormat(obj, field));
            } catch (const DBException& ex) {
                auto status = ex.toStatus();
                if (status != ErrorCodes::NoSuchKey) {
                    throw;
                }
            }
            return boost::none;
        };

        return StaleConfigInfo(NamespaceString(obj["ns"].String()),
                               ChunkVersion::fromBSONLegacyOrNewerFormat(obj, "vReceived"),
                               extractOptionalChunkVersion("vWanted"),
                               ShardId(shardId));
    }

protected:
    NamespaceString _nss;
    ChunkVersion _received;
    boost::optional<ChunkVersion> _wanted;
    ShardId _shardId;

    // This signal does not get serialized and therefore does not get propagated
    // to the router.
    boost::optional<SharedSemiFuture<void>> _criticalSectionSignal;
};

class StaleEpochInfo final : public ErrorExtraInfo {
public:
    static constexpr auto code = ErrorCodes::StaleEpoch;

    StaleEpochInfo(NamespaceString nss) : _nss(std::move(nss)) {}

    const auto& getNss() const {
        return _nss;
    }

    void serialize(BSONObjBuilder* bob) const {
        bob->append("ns", _nss.ns());
    }

    static std::shared_ptr<const ErrorExtraInfo> parse(const BSONObj& obj) {
        return std::make_shared<StaleEpochInfo>(parseFromCommandError(obj));
    }

    static StaleEpochInfo parseFromCommandError(const BSONObj& obj) {
        return StaleEpochInfo(NamespaceString(obj["ns"].String()));
    }

private:
    NamespaceString _nss;
};

using StaleConfigException = ExceptionFor<ErrorCodes::StaleConfig>;

class StaleDbRoutingVersion final : public ErrorExtraInfo {
public:
    static constexpr auto code = ErrorCodes::StaleDbVersion;

    StaleDbRoutingVersion(std::string db,
                          DatabaseVersion received,
                          boost::optional<DatabaseVersion> wanted)
        : _db(std::move(db)), _received(received), _wanted(wanted) {}

    const auto& getDb() const {
        return _db;
    }

    const auto& getVersionReceived() const {
        return _received;
    }

    const auto& getVersionWanted() const {
        return _wanted;
    }

    void serialize(BSONObjBuilder* bob) const override;
    static std::shared_ptr<const ErrorExtraInfo> parse(const BSONObj&);
    static StaleDbRoutingVersion parseFromCommandError(const BSONObj& commandError);

private:
    std::string _db;
    DatabaseVersion _received;
    boost::optional<DatabaseVersion> _wanted;
};

}  // namespace mongo
