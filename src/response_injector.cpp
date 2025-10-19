#include "response_injector.hpp"
#include <nlohmann/json.hpp>
#include <regex>
#include <spdlog/fmt/fmt.h>
#include <algorithm>

using json = nlohmann::json;

namespace lb {

std::string ResponseInjector::inject(const std::string& body,
                                    const std::string& content_type,
                                    uint16_t backend_port) {
    std::string main_type = get_content_type_main(content_type);

    if (main_type.find("html") != std::string::npos) {
        return inject_html(body, backend_port);
    } else if (main_type.find("json") != std::string::npos) {
        return inject_json(body, backend_port);
    } else if (main_type.find("text") != std::string::npos) {
        return inject_text(body, backend_port);
    } else {
        // For other types, return unchanged
        return body;
    }
}

std::string ResponseInjector::inject_html(const std::string& body, uint16_t backend_port) {
    std::string comment = fmt::format("<!-- Served by backend server on port {} -->", backend_port);

    // Find </body> tag (case insensitive)
    std::regex body_tag_regex(R"(</body>)", std::regex_constants::icase);
    std::smatch match;

    std::string result = body;
    if (std::regex_search(result, match, body_tag_regex)) {
        // Insert comment before </body>
        size_t pos = match.position();
        result.insert(pos, comment + "\n");
    } else {
        // If no </body> tag, append at end
        result += "\n" + comment;
    }

    return result;
}

std::string ResponseInjector::inject_json(const std::string& body, uint16_t backend_port) {
    try {
        json j = json::parse(body);

        // Add _server field to root object
        if (j.is_object()) {
            j["_server"] = fmt::format("backend-{}", backend_port);
        } else {
            // If not an object, wrap it
            json wrapped;
            wrapped["data"] = j;
            wrapped["_server"] = fmt::format("backend-{}", backend_port);
            j = wrapped;
        }

        return j.dump();

    } catch (const json::exception&) {
        // If parsing fails, treat as text
        return inject_text(body, backend_port);
    }
}

std::string ResponseInjector::inject_text(const std::string& body, uint16_t backend_port) {
    return body + fmt::format("\n[Served by backend server on port {}]", backend_port);
}

std::string ResponseInjector::get_content_type_main(const std::string& content_type) {
    // Extract main type before semicolon (e.g., "text/html; charset=utf-8" -> "text/html")
    size_t semicolon_pos = content_type.find(';');
    std::string main_type = (semicolon_pos != std::string::npos)
                            ? content_type.substr(0, semicolon_pos)
                            : content_type;

    // Convert to lowercase for comparison
    std::transform(main_type.begin(), main_type.end(), main_type.begin(), ::tolower);

    return main_type;
}

} // namespace lb
