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

#ifndef HEADER_200B4990_6EF5_4516_98E7_41033D1BDCF7_INCLUDED
#define HEADER_200B4990_6EF5_4516_98E7_41033D1BDCF7_INCLUDED

#include <iostream>

#include <boost/exception/all.hpp>
#include <boost/exception/error_info.hpp>

#include <bsoncxx/json.hpp>

#include <mongocxx/exception/operation_exception.hpp>


namespace genny {


/**
 * Wrapper around exceptions thrown by the MongoDB CXX driver to provide more context.
 *
 * More information can be added to MongoException by creating a `typedef boost::error_info`
 * for your info and passing it down from runCommandHelper to MongoException.
 */
class MongoException : public boost::exception, public std::exception {

public:
    using BsonView = bsoncxx::document::view;

    // Dummy MongoException for testing.
    explicit MongoException(const std::string& message = "") {
        *this << MongoException::Message("SOME MESSAGE");
    }

    MongoException(const mongocxx::operation_exception& x,
                   MongoException::BsonView info,
                   const std::string& message = "") {

        *this << MongoException::Info(bsoncxx::to_json(info));

        if (x.raw_server_error() && !x.raw_server_error()->view().empty()) {
            *this << MongoException::ServerError(
                bsoncxx::to_json(x.raw_server_error().value().view()));
        }

        *this << MongoException::Message(message);
    }

private:
    /**
     * boost::error_info are tagged error messages. The first argument is a struct whose name is the
     * tag and the second argument is the type of the error message, the type must be supported by
     * boost::diagnostic_information().
     *
     * You can add a new error_info by streaming it to MongoException (i.e. *this <<
     * MyErrorInfo("uh-oh")).
     */
    using ServerError = boost::error_info<struct ServerResponse, std::string>;
    using Info = boost::error_info<struct InfoObject, std::string>;
    using Message = boost::error_info<struct Message, std::string>;
};
}  // namespace genny

#endif  // HEADER_200B4990_6EF5_4516_98E7_41033D1BDCF7_INCLUDED
