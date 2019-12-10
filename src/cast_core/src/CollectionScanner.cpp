
// Copyright 2019-present MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cast_core/actors/CollectionScanner.hpp>

#include <chrono>
#include <memory>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>

#include <gennylib/Cast.hpp>
#include <gennylib/MongoException.hpp>
#include <gennylib/context.hpp>

#include <value_generators/DocumentGenerator.hpp>

namespace genny::actor {
enum class ScanType { kCount, kSnapshot, kStandard };
enum class SortOrderType { kSortNone, kSortForward, kSortReverse };
// We don't need a metrics clock, as we're using this for measuring
// the end of phase durations and scan durations.
using SteadyClock = std::chrono::steady_clock;

struct CollectionScanner::PhaseConfig {
    // We keep collection names, not collections here, because the
    // names make sense within the context of a database, and we may
    // have multiple databases.
    std::vector<std::string> collectionNames;
    std::optional<DocumentGenerator> filterExpr;
    std::vector<mongocxx::database> databases;
    std::optional<SteadyClock::time_point> stopPhase;
    bool skipFirstLoop = false;
    metrics::Operation scanOperation;
    metrics::Operation exceptionsCaught;
    int64_t documents;
    int64_t scanSizeBytes;
    ScanType scanType;
    SortOrderType sortOrder;
    int64_t collectionSkip;
    std::optional<TimeSpec> scanDuration;
    bool queryCollectionList;
    std::optional<mongocxx::options::transaction> transactionOptions;

    PhaseConfig(PhaseContext& context,
                const CollectionScanner* actor,
                const std::string& databaseNames,
                int collectionCount,
                int threads,
                bool generateCollectionNames)
        : databases{},
          skipFirstLoop{context["SkipFirstLoop"].maybe<bool>().value_or(false)},
          filterExpr{context["Filter"].maybe<DocumentGenerator>(context, actor->id())},
          scanOperation{context.operation("Scan", actor->id())},
          exceptionsCaught{context.operation("ExceptionsCaught", actor->id())},
          documents{context["Documents"].maybe<IntegerSpec>().value_or(0)},
          scanSizeBytes{context["ScanSizeBytes"].maybe<IntegerSpec>().value_or(0)},
          collectionSkip{context["CollectionSkip"].maybe<IntegerSpec>().value_or(0)},
          scanDuration{context["ScanDuration"].maybe<TimeSpec>()} {
        // The list of databases is comma separated.
        std::vector<std::string> dbnames;
        boost::split(dbnames, databaseNames, [](char c) { return (c == ','); });
        for (const auto& dbname : dbnames) {
            databases.push_back((*actor->_client)[dbname]);
        }
        const auto now = SteadyClock::now();
        const auto duration = context["Duration"].maybe<TimeSpec>();
        if (duration) {
            *stopPhase = now + (SteadyClock::duration)*duration;
        }

        // Initialise scan type enum.
        auto scanTypeString = context["ScanType"].to<std::string>();
        // Ignore case
        boost::algorithm::to_lower(scanTypeString);
        if (scanTypeString == "count") {
            scanType = ScanType::kCount;
        } else if (scanTypeString == "snapshot") {
            transactionOptions = mongocxx::options::transaction{};
            auto readConcern = mongocxx::read_concern{};
            readConcern.acknowledge_level(mongocxx::read_concern::level::k_majority);
            transactionOptions->read_concern(readConcern);
            scanType = ScanType::kSnapshot;
        } else if (scanTypeString == "standard") {
            scanType = ScanType::kStandard;
        } else {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("ScanType is invalid."));
        }
        if (scanDuration) {
            if (scanDuration->count() < 0) {
                BOOST_THROW_EXCEPTION(
                    InvalidConfigurationException("ScanDuration must be non-negative."));
            }
            if (scanType != ScanType::kSnapshot) {
                BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                    "ScanDuration must be used with snapshot scans."));
            }
        }
        auto sortOrderString = context["CollectionSortOrder"].maybe<std::string>().value_or("none");
        if (sortOrderString == "none") {
            sortOrder = SortOrderType::kSortNone;
        } else if (sortOrderString == "forward") {
            sortOrder = SortOrderType::kSortForward;
        } else if (sortOrderString == "reverse") {
            sortOrder = SortOrderType::kSortReverse;
        } else {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException("CollectionSortOrder is invalid."));
        }
        if (sortOrder == SortOrderType::kSortNone && collectionSkip != 0) {
            BOOST_THROW_EXCEPTION(InvalidConfigurationException(
                "non-zero CollectionSkip requires a CollectionSortOrder"));
        }

        if (generateCollectionNames) {
            queryCollectionList = false;
            BOOST_LOG_TRIVIAL(info) << " Generating collection names";
            for (const auto& collectionName :
                 distributeCollectionNames(collectionCount, threads, actor->_index)) {
                collectionNames.push_back(collectionName);
            }
        } else {
            queryCollectionList = true;
        }
    }

    void collectionsFromNameList(const mongocxx::database& db,
                                 const std::vector<std::string>& names,
                                 std::vector<mongocxx::collection>& result) {
        std::vector<std::string> nameOrder = names;
        if (sortOrder == SortOrderType::kSortForward) {
            sort(nameOrder.begin(), nameOrder.end(), std::less<>());
        } else if (sortOrder == SortOrderType::kSortReverse) {
            sort(nameOrder.begin(), nameOrder.end(), std::greater<>());
        }
        if (collectionSkip != 0) {
            if (collectionSkip >= nameOrder.size()) {
                return;
            }
            nameOrder =
                std::vector<std::string>(nameOrder.begin() + collectionSkip, nameOrder.end());
        }
        for (const auto& name : nameOrder) {
            auto collection = db[name];
            if (collection) {
                result.push_back(collection);
            }
        }
    }
};

void collectionScan(CollectionScanner::PhaseConfig* config,
                    std::vector<mongocxx::collection>& collections) {
    /*
     * Here we are either doing a snapshot collection scan
     * or just a normal scan?
     */
    size_t docCount = 0;
    size_t scanSize = 0;
    bool scanFinished = false;
    auto statTracker = config->scanOperation.start();
    for (auto& collection : collections) {
        auto filter = config->filterExpr ? config->filterExpr->evaluate()
                                         : bsoncxx::document::view_or_value{};
        auto docs = collection.find(filter);
        /*
         * Try-catch this as the collection may have been deleted.
         * You can still do a find but it'll throw an exception when we iterate.
         */
        try {
            for (auto& doc : docs) {
                docCount += 1;
                scanSize += doc.length();
                if (config->documents != 0 && config->documents == docCount) {
                    scanFinished = true;
                    break;
                }
                if (config->scanSizeBytes != 0 && scanSize >= config->scanSizeBytes) {
                    scanFinished = true;
                    break;
                }
            }
            if (scanFinished) {
                break;
            }
        } catch (const mongocxx::operation_exception& e) {
            auto exceptionsCaught = config->exceptionsCaught.start();
            exceptionsCaught.addDocuments(1);
            exceptionsCaught.success();
        }
    }
    statTracker.addBytes(scanSize);
    statTracker.addDocuments(docCount);
    statTracker.success();
}

void countScan(CollectionScanner::PhaseConfig* config,
               std::vector<mongocxx::collection> collections) {
    auto statTracker = config->scanOperation.start();
    for (auto& collection : collections) {
        auto filter = config->filterExpr ? config->filterExpr->evaluate()
                                         : bsoncxx::document::view_or_value{};
        try {
            statTracker.addDocuments(collection.count_documents(filter));
        } catch (const mongocxx::operation_exception& e) {
            auto exceptionsCaught = config->exceptionsCaught.start();
            exceptionsCaught.addDocuments(1);
            exceptionsCaught.success();
        }
    }
    statTracker.success();
}

void CollectionScanner::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            if (config->skipFirstLoop) {
                config->skipFirstLoop = false;
                std::this_thread::sleep_for(std::chrono::seconds{1});
                continue;
            }
            _runningActorCounter++;
            BOOST_LOG_TRIVIAL(info) << "Starting collection scanner databases: \"" << _databaseNames
                                    << "\", id: " << this->_index;

            // Populate collection names if need be.
            std::vector<mongocxx::collection> collections;
            for (auto database : config->databases) {
                if (config->queryCollectionList) {
                    config->collectionsFromNameList(
                        database, database.list_collection_names({}), collections);
                } else {
                    config->collectionsFromNameList(database, config->collectionNames, collections);
                }
            }

            const SteadyClock::time_point started = SteadyClock::now();

            // Do each kind of scan.
            if (config->scanType == ScanType::kCount) {
                countScan(config, collections);
            } else if (config->scanType == ScanType::kSnapshot) {
                mongocxx::client_session session = _client->start_session({});
                session.start_transaction(*config->transactionOptions);
                collectionScan(config, collections);
                // If a scan duration was specified, we must make the scan
                // last at least that long.  We'll do this within any
                // running transaction, so we keep the "long running
                // transaction" active as long as we've been asked to.
                // However, honor the phase's duration if specified.
                if (config->scanDuration) {
                    const SteadyClock::time_point now = SteadyClock::now();
                    auto stop = started + (*config->scanDuration).value;
                    if (config->stopPhase && *config->stopPhase < stop) {
                        stop = *config->stopPhase;
                    }
                    if (stop > now) {
                        auto sleepDuration = stop - now;
                        auto secs = sleepDuration.count() / (1000 * 1000 * 1000);
                        BOOST_LOG_TRIVIAL(info)
                            << "Scanner id: " << this->_index << " sleeping " << secs;
                        std::this_thread::sleep_for(sleepDuration);
                    }
                }
                session.commit_transaction();

            } else {
                collectionScan(config, collections);
            }

            _runningActorCounter--;
            BOOST_LOG_TRIVIAL(info) << "Finished collection scanner id: " << this->_index;
        }
    }
}

CollectionScanner::CollectionScanner(genny::ActorContext& context)
    : Actor{context},
      _totalInserts{context.operation("Insert", CollectionScanner::id())},
      _client{context.client()},
      _index{WorkloadContext::getActorSharedState<CollectionScanner, ActorCounter>().fetch_add(1)},
      _runningActorCounter{
          WorkloadContext::getActorSharedState<CollectionScanner, RunningActorCounter>()},
      _generateCollectionNames{context["GenerateCollectionNames"].maybe<bool>().value_or(false)},
      _databaseNames{context["Database"].to<std::string>()},
      _loop{context,
            this,
            _databaseNames,
            context["CollectionCount"].maybe<IntegerSpec>().value_or(0),
            context["Threads"].to<IntegerSpec>(),
            context["GenerateCollectionNames"].maybe<bool>().value_or(false)} {
    _runningActorCounter.store(0);
}

std::vector<std::string> distributeCollectionNames(size_t collectionCount,
                                                   size_t threadCount,
                                                   ActorId actorId) {
    // We always want a fair division of collections to actors
    std::vector<std::string> collectionNames{};
    if ((threadCount > collectionCount && threadCount % collectionCount != 0) ||
        collectionCount % threadCount != 0) {
        throw std::invalid_argument("Thread count must be mutliple of database collection count");
    }
    int collectionsPerActor = threadCount > collectionCount ? 1 : collectionCount / threadCount;
    int collectionIndexStart = (actorId % collectionCount) * collectionsPerActor;
    int collectionIndexEnd = collectionIndexStart + collectionsPerActor;
    for (int i = collectionIndexStart; i < collectionIndexEnd; ++i) {
        collectionNames.push_back("Collection" + std::to_string(i));
    }
    return collectionNames;
}

namespace {
auto registerCollectionScanner = Cast::registerDefault<CollectionScanner>();
}  // namespace
}  // namespace genny::actor
