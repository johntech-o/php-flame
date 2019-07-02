#include "../coroutine.h"
#include "../time/time.h"
#include "_handler.h"
#include "http.h"
#include "server.h"
#include "server_request.h"
#include "server_response.h"

namespace flame::http {

    _handler::_handler(server *svr, tcp::socket &&sock)
    : svr_ptr(svr)
    , svr_obj(svr)
    , socket_(std::move(sock)) {
    }

    _handler::~_handler() {
    }

    void _handler::start() {
        req_.reset();
        res_.reset();
        if ((req_ && !req_->keep_alive()) || (res_ && !res_->keep_alive()))  return;
        // 第一次请求或允许复用
        read_request();
    }

    void _handler::read_request() {
        // 读取请求
        req_.reset(new boost::beast::http::request_parser<value_body<true>>());
        req_->header_limit(16 * 1024);
        req_->body_limit(body_max_size);
        boost::beast::http::async_read(socket_, buffer_, *req_
            , [self = shared_from_this(), this] (const boost::system::error_code &error, std::size_t n) {

            if (error == boost::beast::http::error::end_of_stream
                || error == boost::asio::error::operation_aborted
                || error == boost::asio::error::connection_reset) {

                res_.reset();
                req_.reset();
                return;
            }
            else if (error) {
                res_.reset();
                req_.reset();
                // std::cerr << "[" << time::iso() << "] (WARNING) Failed to read http request: (" << error.value() << ") " << error.message() << std::endl;
                return;
            }
            res_.reset(new boost::beast::http::message<false, value_body<false>>());
            // 默认为请求 KeepAlive 配置
            res_->keep_alive(req_->get().keep_alive());
            // 默认应为非 chunked 模式
            assert(!res_->chunked());
            call_handler();
        });
    }

    void _handler::call_handler() {
        php::object req = php::object(php::class_entry<server_request>::entry());
        php::object res = php::object(php::class_entry<server_response>::entry());
        server_request *req_ptr = static_cast<server_request *>(php::native(req));
        server_response *res_ptr = static_cast<server_response *>(php::native(res));

        req_ptr->build_ex(req_->get()); // 将 C++ 请求对象展开到 PHP 中
        res_ptr->handler_ = shared_from_this();

        coroutine::start(php::callable([self = shared_from_this(), this, req, req_ptr, res, res_ptr] (php::parameters& params) -> php::value {
            coroutine_handler ch{coroutine::current};
            std::string route = req.get("method");
            route.push_back(':');
            route.append(req.get("path"));

            auto ib = svr_ptr->cb_.find("before"),
                ih = svr_ptr->cb_.find(route),
                ia = svr_ptr->cb_.find("after");

            try {
                php::value rv = true;
                if (ib != svr_ptr->cb_.end()) rv = ib->second.call({req, res, ih != svr_ptr->cb_.end()});
                if (rv.type_of(php::TYPE::NO)) goto HANDLE_BREAK;
                if (ih != svr_ptr->cb_.end()) rv = ih->second.call({req, res});
                if (rv.type_of(php::TYPE::NO)) goto HANDLE_BREAK;
                if (ia != svr_ptr->cb_.end()) rv = ia->second.call({req, res, ih != svr_ptr->cb_.end()});
HANDLE_BREAK:
                if (!res_->chunked()/* && !(res_ptr->status_ & server_response::STATUS_BODY_SENT)*/) {
                    // if (!(res_ptr->status_ & server_response::STATUS_HEAD_SENT)) {
                    //     res_ptr->set("status", 404);
                    //     res_ptr->set("body", nullptr);
                    // }
                    // 服务及将停止或服务器以进行关闭后，禁止连接复用
                    if (gcontroller->status & controller::STATUS_CLOSECONN || svr_ptr->closed_) {
                        res_->keep_alive(false);
                        res_->set(boost::beast::http::field::connection, "close");
                    }
                    res_ptr->build_ex(*res_);
                    // Content 方式, 待结束
                    php::string body = res.get("body");
                    if (!body.empty())
                        body = ctype_encode(res_->find(boost::beast::http::field::content_type)->value(), body);
                    res_->body() = body;
                    res_->prepare_payload();

                    boost::system::error_code error;
                    boost::beast::http::async_write(socket_, *res_, ch[error]);
                    if (error) {
                        res_.reset();
                        req_.reset();
                        throw php::exception(zend_ce_exception
                            , (boost::format("Failed to write response: %s") % error.message()).str()
                            , error.value());
                    }
                    else start();
                }
                /*else {
                    // Chunked 未结束 -> server_response::__destruct() -> finish()
                }*/
            } catch(const php::exception& ex) {
                res_.reset();
                req_.reset();
                // 调用用户异常回调
                gcontroller->call_user_cb("exception", {ex});
                // 记录错误信息
                php::object obj = ex;
                std::cerr << "[" << time::iso() << "] (ERROR) Uncaught Exception in HTTP handler: " << obj.call("__toString") << "\n";
            }
            return nullptr;
        }));
    }

    void _handler::write_head(server_response *res_ptr, coroutine_handler &ch) {
        if (!res_) return;
        res_ptr->build_ex(*res_);
        res_->chunked(true);
        res_->set(boost::beast::http::field::transfer_encoding, "chunked");
        // 服务及将停止或服务器以进行关闭后，禁止连接复用
        if (gcontroller->status & controller::STATUS_CLOSECONN || svr_ptr->closed_) {
            res_->keep_alive(false);
            res_->set(boost::beast::http::field::connection, "close");
        }
        boost::system::error_code error;

        boost::beast::http::serializer<false, value_body<false>> sr(*res_);
        boost::beast::http::async_write_header(socket_, sr, ch[error]);
        if (error) {
            res_.reset();
            req_.reset();
            throw php::exception(zend_ce_exception
                , (boost::format("Failed to write header: %s") % error.message()).str()
                , error.value());
        }
    }

    void _handler::write_body(server_response *res_ptr, php::string data, coroutine_handler &ch) {
        if (!res_) return;
        if (!res_->chunked())
            throw php::exception(zend_ce_error_exception
                , "Failed to write_body: 'chunked' encoding required"
                , -1);

        boost::system::error_code error;
        boost::asio::async_write(socket_,
            boost::beast::http::make_chunk(boost::asio::const_buffer(data.c_str(), data.size())), ch[error]);
        if (error) {
            res_.reset();
            req_.reset();
            throw php::exception(zend_ce_exception
                , (boost::format("Failed to write body: %s") % error.message()).str()
                , error.value());
        }
    }

    void _handler::write_end(server_response* res_ptr, coroutine_handler &ch) {
        if (!res_) return;
        if (!res_->chunked()) throw php::exception(zend_ce_error_exception
            , "Failed to write body: 'chunked' encoding required"
            , -1);
        boost::system::error_code error;
        boost::asio::async_write(socket_, boost::beast::http::make_chunk_last(), ch[error]);
        if (error) {
            res_.reset();
            req_.reset();
            throw php::exception(zend_ce_exception
                , (boost::format("Failed to write body: %s") % error.message()).str()
                , error.value());
        }
    }

    void _handler::write_file(server_response *res_ptr, std::string path, coroutine_handler &ch) {
        if (!res_) return;
        if (!res_->chunked())
            throw php::exception(zend_ce_error_exception
                , "Failed to write_body: 'chunked' encoding required"
                , -1);
        std::ifstream file(path);
        char buffer[2048];
        std::size_t size;
        boost::system::error_code error;

        while (!file.eof()) {
            boost::asio::post(gcontroller->context_y, [&file, &ch, &buffer, &size, &error] () {
                try {
                    file.read(buffer, sizeof(buffer));
                    size = file.gcount();
                } catch(const std::ios_base::failure& ex) {
                    error.assign(ex.code().value(), boost::system::generic_category());
                }
                ch.resume();
            });
            ch.suspend();
            if (error) {
                res_.reset();
                req_.reset();
                throw php::exception(zend_ce_exception
                    , (boost::format("Failed to write file: %s") % error.message()).str()
                    , error.value());
            }
            boost::asio::async_write(socket_, boost::beast::http::make_chunk( boost::asio::buffer(buffer, size) ), ch[error]);
            if (error) goto WRITE_ERROR;
        }
        boost::asio::async_write(socket_, boost::beast::http::make_chunk_last(), ch[error]);
WRITE_ERROR:
        if (error) {
            res_.reset();
            req_.reset();
            throw php::exception(zend_ce_exception
                , (boost::format("Failed to write file: %s") % error.message()).str()
                , error.value());
        }
        // file.close();
    }

    void _handler::finish(server_response *res_ptr, coroutine_handler &ch) {
        if (res_) {
            if (res_->chunked() && !(res_ptr->status_ & server_response::STATUS_BODY_END)) {
                boost::system::error_code error;
                boost::asio::async_write(socket_, boost::beast::http::make_chunk_last(), ch[error]);
                res_.reset();
                req_.reset();
                if (error) std::cerr << "[" << time::iso() << "] (ERROR) Failed to write body: (" 
                    << error.value() << ") " << error.message() << "\n";
                else start();
            }
        }
    }
}
