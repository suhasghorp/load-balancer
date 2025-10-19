#pragma once

#include <string>
#include <cstdint>

namespace lb {

class ResponseInjector {
public:
    // Inject backend port information into response body based on content type
    static std::string inject(const std::string& body,
                             const std::string& content_type,
                             uint16_t backend_port);

private:
    static std::string inject_html(const std::string& body, uint16_t backend_port);
    static std::string inject_json(const std::string& body, uint16_t backend_port);
    static std::string inject_text(const std::string& body, uint16_t backend_port);
    static std::string get_content_type_main(const std::string& content_type);
};

} // namespace lb
