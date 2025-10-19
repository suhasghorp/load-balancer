#include <gtest/gtest.h>
#include "response_injector.hpp"

using namespace lb;

class ResponseInjectorTest : public ::testing::Test {};

TEST_F(ResponseInjectorTest, InjectHTML) {
    std::string html = R"(<!DOCTYPE html>
<html>
<head><title>Test</title></head>
<body>
<h1>Hello World</h1>
</body>
</html>)";

    std::string result = ResponseInjector::inject(html, "text/html", 8080);

    EXPECT_NE(result.find("<!-- Served by backend server on port 8080 -->"), std::string::npos);
    EXPECT_NE(result.find("</body>"), std::string::npos);

    // Comment should be before </body>
    size_t comment_pos = result.find("<!-- Served by backend server on port 8080 -->");
    size_t body_pos = result.find("</body>");
    EXPECT_LT(comment_pos, body_pos);
}

TEST_F(ResponseInjectorTest, InjectHTMLNoBodyTag) {
    std::string html = "<h1>Hello World</h1>";
    std::string result = ResponseInjector::inject(html, "text/html", 8081);

    // Should append at end if no </body> tag
    EXPECT_NE(result.find("<!-- Served by backend server on port 8081 -->"), std::string::npos);
}

TEST_F(ResponseInjectorTest, InjectJSON) {
    std::string json = R"({"message": "Hello", "data": [1, 2, 3]})";
    std::string result = ResponseInjector::inject(json, "application/json", 8082);

    EXPECT_NE(result.find("\"_server\""), std::string::npos);
    EXPECT_NE(result.find("\"backend-8082\""), std::string::npos);
    EXPECT_NE(result.find("\"message\""), std::string::npos);
}

TEST_F(ResponseInjectorTest, InjectJSONArray) {
    std::string json = R"([1, 2, 3])";
    std::string result = ResponseInjector::inject(json, "application/json", 8083);

    // Should wrap array in object
    EXPECT_NE(result.find("\"_server\""), std::string::npos);
    EXPECT_NE(result.find("\"backend-8083\""), std::string::npos);
    EXPECT_NE(result.find("\"data\""), std::string::npos);
}

TEST_F(ResponseInjectorTest, InjectText) {
    std::string text = "Hello World\nThis is a test";
    std::string result = ResponseInjector::inject(text, "text/plain", 8084);

    EXPECT_NE(result.find("[Served by backend server on port 8084]"), std::string::npos);

    // Should be at the end
    size_t injection_pos = result.find("[Served by backend server on port 8084]");
    EXPECT_GT(injection_pos, 0);
}

TEST_F(ResponseInjectorTest, InjectUnknownType) {
    std::string binary = "Some binary data";
    std::string result = ResponseInjector::inject(binary, "application/octet-stream", 8085);

    // Should return unchanged for unknown types
    EXPECT_EQ(result, binary);
}

TEST_F(ResponseInjectorTest, ContentTypeWithCharset) {
    std::string html = "<html><body>Test</body></html>";
    std::string result = ResponseInjector::inject(html, "text/html; charset=utf-8", 8086);

    // Should still detect HTML despite charset
    EXPECT_NE(result.find("<!-- Served by backend server on port 8086 -->"), std::string::npos);
}

TEST_F(ResponseInjectorTest, InvalidJSON) {
    std::string invalid_json = "{invalid json}";
    std::string result = ResponseInjector::inject(invalid_json, "application/json", 8087);

    // Should fallback to text injection when JSON parsing fails
    EXPECT_NE(result.find("[Served by backend server on port 8087]"), std::string::npos);
}
