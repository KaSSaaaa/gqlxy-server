#pragma once
#include "GraphQLWSListener.h"

#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp/core/data/mapping/ObjectMapper.hpp>
#include <oatpp/core/macro/component.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>

namespace gqlxy::server::internal {

class AppComponents {
public:
    AppComponents(const Schema& schema) {
        wsHandler.getObject()->setSocketInstanceListener(std::make_shared<GraphQLWSInstanceListener>(schema));
    }
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper)(
        oatpp::parser::json::mapping::ObjectMapper::createShared());
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::websocket::ConnectionHandler>, wsHandler)(
        oatpp::websocket::ConnectionHandler::createShared());
};

}