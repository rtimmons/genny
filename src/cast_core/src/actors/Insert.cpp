#include <cast_core/actors/Insert.hpp>

#include <memory>

#include <yaml-cpp/yaml.h>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>

#include <boost/log/trivial.hpp>

#include <bsoncxx/json.hpp>
#include <gennylib/Cast.hpp>
#include <gennylib/context.hpp>
#include <gennylib/value_generators.hpp>

namespace genny::actor {

/** @private */
struct Insert::PhaseConfig {
    mongocxx::collection collection;
    std::unique_ptr<value_generators::DocumentGenerator> json_document;
    ExecutionStrategy::RunOptions options;

    PhaseConfig(PhaseContext& phaseContext, genny::DefaultRandom& rng, const mongocxx::database& db)
        : collection{db[phaseContext.get<std::string>("Collection")]},
          json_document{value_generators::makeDoc(phaseContext.get("Document"), rng)},
          options{ExecutionStrategy::getOptionsFrom(phaseContext, "ExecutionsStrategy")} {}
};

void Insert::run() {
    for (auto&& config : _loop) {
        for (const auto&& _ : config) {
            _strategy.run(
                [&]() {
                    bsoncxx::builder::stream::document mydoc{};
                    auto view = config->json_document->view(mydoc);
                    BOOST_LOG_TRIVIAL(info) << " Inserting " << bsoncxx::to_json(view);
                    config->collection.insert_one(view);
                },
                config->options);
        }

        _strategy.recordMetrics();
    }
}

Insert::Insert(genny::ActorContext& context)
    : Actor(context),
      _rng{context.workload().createRNG()},
      _strategy{context, Insert::id(), "insert"},
      _client{std::move(context.client())},
      _loop{context, _rng, (*_client)[context.get<std::string>("Database")]} {}

namespace {
auto registerInsert = genny::Cast::registerDefault<genny::actor::Insert>();
}
}  // namespace genny::actor
