#include "workloadNode.hpp"
#include <stdlib.h>
#include <boost/log/trivial.hpp>
#include "workload.hpp"

namespace mwg {

workloadNode::workloadNode(YAML::Node& ynode) : node(ynode) {
    if (ynode["type"].Scalar() != "workloadNode") {
        BOOST_LOG_TRIVIAL(fatal)
            << "ForN constructor but yaml entry doesn't have type == workloadNode";
        exit(EXIT_FAILURE);
    }
    if (!ynode["workload"]) {
        BOOST_LOG_TRIVIAL(fatal) << "ForN constructor but yaml entry doesn't have a workload entry";
        exit(EXIT_FAILURE);
    }
    auto yamlWorkload = ynode["workload"];
    myWorkload = unique_ptr<workload>(new workload(yamlWorkload));
}

// Execute the node
void workloadNode::execute(shared_ptr<threadState> myState) {
    myWorkload->uri = myState->myWorkload.uri;
    chrono::high_resolution_clock::time_point start, stop;
    BOOST_LOG_TRIVIAL(debug) << "In workloadNode and executing";
    start = chrono::high_resolution_clock::now();
    myWorkload->execute(myState->conn);
    stop = chrono::high_resolution_clock::now();
    BOOST_LOG_TRIVIAL(debug) << "Node " << name << " took "
                             << std::chrono::duration_cast<chrono::microseconds>(stop - start)
                                    .count() << " microseconds";
}
std::pair<std::string, std::string> workloadNode::generateDotGraph() {
    return (std::pair<std::string, std::string>{name + " -> " + nextName + ";\n",
                                                myWorkload->generateDotGraph()});
}
}
