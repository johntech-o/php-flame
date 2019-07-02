#include "../controller.h"
#include "log.h"
#include "../time/time.h"

namespace flame::log {

    static std::string LEVEL_S[] = {
        "TRACE",
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "FATAL",
    };

    static std::map<std::string, int> LEVEL_I {};

    int level = 0;

    static void write_ex(int lv, php::parameters& params) {
        if (lv < level) return;
        std::ostream& os = lv > LEVEL_WARNING ? std::cout : std::cerr;
        os << '[' << time::iso() << "] (";
        os << LEVEL_S[lv];
        os << ")";
        for (int i = 0; i < params.size(); ++i)
            os << ' ' << params[i].ptr();

        os << std::endl;
    }

    static php::value trace(php::parameters &params) {
        write_ex(LEVEL_TRACE, params);
        return nullptr;
    }

    static php::value debug(php::parameters &params) {
        write_ex(LEVEL_DEBUG, params);
        return nullptr;
    }

    static php::value info(php::parameters &params) {
        write_ex(LEVEL_INFO, params);
        return nullptr;
    }

    static php::value warning(php::parameters &params) {
        write_ex(LEVEL_WARNING, params);
        return nullptr;
    }

    static php::value error(php::parameters &params) {
        write_ex(LEVEL_ERROR, params);
        return nullptr;
    }

    static php::value fatal(php::parameters &params) {
        write_ex(LEVEL_FATAL, params);
        return nullptr;
    }

    void declare(php::extension_entry &ext) {
        gcontroller->on_init([] (const php::array& options) {
            if (options.exists("level")) level = options.get("level").to_integer();
            else level = 0;
        });
        ext
            .function<trace>("flame\\log\\trace")
            .function<debug>("flame\\log\\debug")
            .function<info>("flame\\log\\info")
            .function<warning>("flame\\log\\warn")
            .function<warning>("flame\\log\\warning")
            .function<error>("flame\\log\\error")
            .function<fatal>("flame\\log\\fail")
            .function<fatal>("flame\\log\\fatal");
    }
}
