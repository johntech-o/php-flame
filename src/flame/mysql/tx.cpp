#include "../coroutine.h"
#include "tx.h"
#include "_connection_lock.h"
#include "mysql.h"

namespace flame::mysql {
    void tx::declare(php::extension_entry &ext) {
        php::class_entry<tx> class_tx("flame\\mysql\\tx");
        class_tx
            .method<&tx::__construct>("__construct", {}, php::PRIVATE)
            .method<&tx::__destruct>("__destruct")
            .method<&tx::escape>("escape", {
                {"data", php::TYPE::UNDEFINED},
            })
            .method<&tx::commit>("commit")
            .method<&tx::rollback>("rollback")
            .method<&tx::query>("query", {
                {"sql", php::TYPE::STRING},
            })
            .method<&tx::insert>("insert", {
                {"table", php::TYPE::STRING},
                {"rows", php::TYPE::ARRAY},
            })
            .method<&tx::delete_>("delete", {
                {"table", php::TYPE::STRING},
                {"where", php::TYPE::UNDEFINED},
                {"order", php::TYPE::UNDEFINED, false, true},
                {"limit", php::TYPE::UNDEFINED, false, true},
            })
            .method<&tx::update>("update", {
                {"table", php::TYPE::STRING},
                {"where", php::TYPE::UNDEFINED},
                {"set", php::TYPE::UNDEFINED},
                {"order", php::TYPE::UNDEFINED, false, true},
                {"limit", php::TYPE::UNDEFINED, false, true},
            })
            .method<&tx::select>("select", {
                {"table", php::TYPE::STRING},
                {"fields", php::TYPE::UNDEFINED},
                {"where", php::TYPE::UNDEFINED},
                {"order", php::TYPE::UNDEFINED, false, true},
                {"limit", php::TYPE::UNDEFINED, false, true},
            })
            .method<&tx::one>("one", {
                {"table", php::TYPE::STRING},
                {"where", php::TYPE::UNDEFINED},
                {"order", php::TYPE::UNDEFINED, false, true},
            })
            .method<&tx::get>("get", {
                {"table", php::TYPE::STRING},
                {"field", php::TYPE::STRING},
                {"where", php::TYPE::UNDEFINED},
                {"order", php::TYPE::UNDEFINED, false, true},
            });
        ext.add(std::move(class_tx));
    }

    php::value tx::__construct(php::parameters &params) {
        return nullptr;
    }

    php::value tx::__destruct(php::parameters &params) {
        if (!done_) {
            // 强制结束时，上下文已不存在，不能进行协程恢复
            // 不能使用 this->rollback(params);
            cl_->rollback();
        }
        return nullptr;
    }

    php::value tx::escape(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        std::shared_ptr<MYSQL> conn = cl_->acquire(ch);

        php::buffer buffer;
        char quote = '\'';
        if (params.size() > 1 && params[1].type_of(php::TYPE::STRING)) {
            php::string q = params[1];
            if (q.data()[0] == '`') quote = '`';
        }
        _connection_base::escape(conn, buffer, params[0], quote);
        return std::move(buffer);
    }

    php::value tx::commit(php::parameters &params) {
        done_ = true;
        coroutine_handler ch{coroutine::current};
        cl_->commit(ch);
        return nullptr;
    }

    php::value tx::rollback(php::parameters &params) {
        done_ = true;
        coroutine_handler ch{coroutine::current};
        cl_->rollback(ch);
        return nullptr;
    }

    php::value tx::query(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        if(params.size() <= 1) // 单参数时的处理方式
            return cl_->query(cl_->acquire(ch), params[0], ch);
        // 多参数进行格式化
        auto conn = cl_->acquire(ch);
        return cl_->query(conn, cl_->format(conn, params), ch);
    }

    php::value tx::insert(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        auto conn = cl_->acquire(ch);
        php::buffer buf;
        build_insert(conn, buf, params);
        return cl_->query(conn, std::string(buf.data(), buf.size()), ch);
    }

    php::value tx::delete_(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        auto conn = cl_->acquire(ch);
        php::buffer buf;
        build_delete(conn, buf, params);
        return cl_->query(conn, std::string(buf.data(), buf.size()), ch);
    }

    php::value tx::update(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        auto conn = cl_->acquire(ch);
        php::buffer buf;
        build_update(conn, buf, params);
        return cl_->query(conn, std::string(buf.data(), buf.size()), ch);
    }

    php::value tx::select(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        auto conn = cl_->acquire(ch);
        php::buffer buf;
        build_select(conn, buf, params);
        return cl_->query(conn, std::string(buf.data(), buf.size()), ch);
    }

    php::value tx::one(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        auto conn = cl_->acquire(ch);
        php::buffer buf;
        build_one(conn, buf, params);
        php::object rst = cl_->query(conn, std::string(buf.data(), buf.size()), ch);
        return rst.call("fetch_row");
    }

    php::value tx::get(php::parameters &params) {
        coroutine_handler ch{coroutine::current};
        auto conn = cl_->acquire(ch);
        php::buffer buf;
        build_get(conn, buf, params);
        php::object rst = cl_->query(conn, std::string(buf.data(), buf.size()), ch);
        php::array row = rst.call("fetch_row");
        if (!row.empty()) return row.get( static_cast<php::string>(params[1]) );
        else return nullptr;
    }
} // namespace flame::mysql
