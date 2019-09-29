#pragma once
#include "../../vendor.h"
#include "mongodb.h"

namespace flame::mongodb {

    class _connection_pool;
    class collection : public php::class_base {
    public:
        static void declare(php::extension_entry &ext);
        php::value __construct(php::parameters &params);
        php::value insert(php::parameters &params);
        php::value delete_(php::parameters &params);
        php::value delete_one(php::parameters& params);
        php::value update(php::parameters &params);
        php::value update_one(php::parameters& params);
        php::value find(php::parameters &params);
        php::value find_many(php::parameters &params);
        php::value find_one(php::parameters &params);
        php::value get(php::parameters &params);
        php::value count(php::parameters &params);
        php::value aggregate(php::parameters &params);
        php::value find_and_update(php::parameters& params);
        php::value find_and_delete(php::parameters& params);
    private:
        std::shared_ptr<_connection_pool> cp_;
        php::string name_;
        friend class client;
    };
} // namespace flame::mongodb
