#pragma once
#include "../vendor.h"
#include <amqpcpp.h>
#include <amqpcpp/libboostasio.h>

namespace flame::rabbitmq {

    void declare(php::extension_entry &ext);
    php::value consume(php::parameters &params);
    php::value produce(php::parameters &params);

    php::array  table2array(const AMQP::Table &table);
    AMQP::Table array2table(const php::array &table);
} // namespace flame::rabbitmq
