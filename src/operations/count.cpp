#include "count.hpp"
#include "parse_util.hpp"
#include <bsoncxx/json.hpp>
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include <mongocxx/exception/base.hpp>

namespace mwg {

count::count(YAML::Node& node) {
    // need to set the name
    // these should be made into exceptions
    // should be a map, with type = count
    if (!node) {
        BOOST_LOG_TRIVIAL(fatal) << "Count constructor and !node";
        exit(EXIT_FAILURE);
    }
    if (!node.IsMap()) {
        BOOST_LOG_TRIVIAL(fatal) << "Not map in count type initializer";
        exit(EXIT_FAILURE);
    }
    if (node["type"].Scalar() != "count") {
        BOOST_LOG_TRIVIAL(fatal) << "Count constructor but yaml entry doesn't have type == count";
        exit(EXIT_FAILURE);
    }
    filter = makeDoc(node["filter"]);
    BOOST_LOG_TRIVIAL(debug) << "Added op of type count";
    if (node["options"])
        parseCountOptions(options, node["options"]);
}

// Execute the node
void count::execute(mongocxx::client& conn, threadState& state) {
    auto collection = conn["testdb"]["testCollection"];
    bsoncxx::builder::stream::document mydoc{};
    auto view = filter->view(mydoc, state);
    try {
        auto returnCount = collection.count(view, options);
        BOOST_LOG_TRIVIAL(debug) << "count.execute: filter is " << bsoncxx::to_json(view)
                                 << " and count is " << returnCount << endl;
    } catch (mongocxx::exception::base e) {
        BOOST_LOG_TRIVIAL(error) << "Caught mongo exception in count: " << e.what();
        auto error = e.raw_server_error();
        if (error)
            BOOST_LOG_TRIVIAL(error) << bsoncxx::to_json(error->view());
        auto errorandcode = e.error_and_code();
        if (errorandcode)
            BOOST_LOG_TRIVIAL(error) << "Error code is " << get<1>(errorandcode.value()) << " and "
                                     << get<0>(errorandcode.value());
    }
}
}
