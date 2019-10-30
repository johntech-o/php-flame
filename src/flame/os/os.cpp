#include "../coroutine.h"
#include "../controller.h"
#include "os.h"
#include "process.h"
#include <filesystem>
// 获取本地网卡地址
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <boost/process.hpp>
#include <boost/process/async.hpp>

namespace flame::os {
    void declare(php::extension_entry &ext) {
        ext
            .function<interfaces>("flame\\os\\interfaces")
            .function<spawn>("flame\\os\\spawn", {
                {"executable", php::TYPE::STRING},
                {"arguments", php::TYPE::ARRAY, false, true},
                {"options", php::TYPE::ARRAY, false, true},
            })
            .function<exec>("flame\\os\\exec", {
                {"executable", php::TYPE::STRING},
                {"arguments", php::TYPE::ARRAY, false, true},
                {"options", php::TYPE::ARRAY, false, true},
            });

        process::declare(ext);
    }

    static void interface(php::array& data, const struct ifaddrs *addr) {
        php::string name(addr->ifa_name);
        php::array info(2);
        info.set("family", addr->ifa_addr->sa_family == AF_INET ? "IPv4" : "IPv6");
        char address[NI_MAXHOST];
        if (getnameinfo(addr->ifa_addr,
                addr->ifa_addr->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
                address, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) != 0)
            throw php::exception(zend_ce_exception, gai_strerror(errno), errno);
        
        info.set("address", address);
        if (data.exists(name)) {
            php::array iface = data.get(name);
            iface.set(iface.size(), info);
        }
        else {
            php::array iface(2);
            iface.set(iface.size(), info);
            data.set(name, iface);
        }
    }

    php::value interfaces(php::parameters &params) {
        struct ifaddrs *addr;
        if (getifaddrs(&addr) != 0) throw php::exception(zend_ce_exception, std::strerror(errno), errno);
        else if (!addr) return php::array(0);
       
        php::array data(4);
        // 用 shared_ptr 作自动释放保证
        std::shared_ptr<struct ifaddrs> autofree(addr, freeifaddrs);
        do {
            if (addr->ifa_addr->sa_family != AF_INET && addr->ifa_addr->sa_family != AF_INET6) continue;
            interface(data, addr);
        } while ((addr = addr->ifa_next) != nullptr);
        return std::move(data);
    }

    php::value spawn(php::parameters &params) {
        php::object proc(php::class_entry<process>::entry());
        process *proc_ = static_cast<process *>(php::native(proc));
        proc_->exit_ = false;

        auto env = boost::this_process::environment();
        std::string exec = params[0].to_string();
        if (exec[0] != '.' && exec[0] != '/')
            exec = boost::process::search_path(exec).native();
        
        std::vector<std::string> argv;
        std::string cwdv = boost::filesystem::current_path().native();
        if (params.size() > 1) {
            php::array args = params[1];
            for (auto i = args.begin(); i != args.end(); ++i)
                argv.push_back(i->second.to_string());
        }
        if (params.size() > 2) {
            php::array opts = params[2];
            php::array envs = opts.get("env");
            if (envs.type_of(php::TYPE::ARRAY)) {
                for (auto i = envs.begin(); i != envs.end(); ++i)
                    env[i->first.to_string()] = i->second.to_string();
            }
            php::string cwds = opts.get("cwd");
            if (cwds.type_of(php::TYPE::STRING)) cwdv = cwds.to_string();
        }
        auto c = std::make_unique<boost::process::child>(
            exec, boost::process::args = argv, env, gcontroller->context_x,
            boost::process::start_dir = cwdv,
            boost::process::std_out > proc_->out_,
            boost::process::std_err > proc_->err_,
            boost::process::on_exit = [proc, proc_] (int exit_code, const std::error_code &) {
                proc_->exit_ = true;
                if (proc_->ch_) proc_->ch_.resume();
            });
        if(c->valid()) {
            proc.set("pid", c->id());
            proc_->c_ = std::move(c);
            return proc;
        }
        else {
            return nullptr;
        }
    }

    php::value exec(php::parameters &params) {
        php::object proc = spawn(params);
        process *proc_ = static_cast<process *>(php::native(proc));
        return proc.call("stdout");
    }
} // namespace flame::os
