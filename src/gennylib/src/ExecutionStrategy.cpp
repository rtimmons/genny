#include <gennylib/ExecutionStrategy.hpp>

#include <boost/log/trivial.hpp>

#include <gennylib/context.hpp>

namespace genny {
ExecutionStrategy::ExecutionStrategy(ActorContext& context,
                                     ActorId id,
                                     const std::string& operation)
    : _errorGauge{context.gauge(operation + ".errors", id)},
      _opsGauge{context.gauge(operation + ".ops", id)},
      _timer{context.timer(operation + ".op-time", id)} {}

ExecutionStrategy::~ExecutionStrategy() {
    recordMetrics();
}

void ExecutionStrategy::recordMetrics() {
    _opsGauge.set(_ops);
}

void ExecutionStrategy::_recordError(const mongocxx::operation_exception& e) {
    BOOST_LOG_TRIVIAL(info) << "Error #" << _errors << ": " << e.what();

    _errorGauge.set(++_errors);
}

void ExecutionStrategy::_finishRun(const RunOptions& options, Result result) {
    if (!result.wasSuccessful) {
        BOOST_LOG_TRIVIAL(error) << "Operation failed after " << options.maxRetries
                                 << " retry attempts.";
    }

    _lastResult = std::move(result);
}
}  // namespace genny
