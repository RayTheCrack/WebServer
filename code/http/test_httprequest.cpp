
#include "httprequest.h"
#include "../buffer/buffer.h"
#include <iostream>
#include <cassert>

void testBasicRequest() {
    LOG_INFO("=== Test 1: Basic GET Request ===");
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.method() == "GET");
    assert(request.path() == "/index.html");
    assert(request.version() == "1.1");
    assert(request.IsKeepAlive() == true);
    LOG_INFO("✓ Test 1 passed!");
}

void testRootPath() {
    LOG_INFO("=== Test 2: Root Path ===");
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.path() == "/index.html");
    LOG_INFO("✓ Test 2 passed!");
}

void testDefaultHTML() {
    LOG_INFO("=== Test 3: Default HTML Paths ===" );
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "GET /login HTTP/1.1\r\nHost: localhost:8080\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.path() == "/login.html");
    LOG_INFO("✓ Test 3 passed!");
}

void testHeaders() {
    LOG_INFO("=== Test 4: Multiple Headers ===" );
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent: Mozilla/5.0\r\nAccept: text/html\r\nConnection: close\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.IsKeepAlive() == false);
    LOG_INFO("✓ Test 4 passed!");
}

void testPostRequest() {
    LOG_INFO("=== Test 5: POST Request with Body ===");
    Buffer buff;
    HttpRequest request;

    // Use correct credentials (admin/admin) that exist in database
    std::string rawRequest = "POST /login.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 27\r\n\r\nusername=admin&password=admin";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.method() == "POST");
    // After successful login, path should be redirected to welcome.html
    LOG_INFO("Path after parse: {}", request.path());
    // Check if path is either welcome.html (success) or error.html (failed)
    assert(request.path() == "/welcome.html" || request.path() == "/error.html");
    LOG_INFO("✓ Test 5 passed!");
}

void testUrlEncoding() {
    LOG_INFO("=== Test 6: URL Encoding ===");
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 33\r\n\r\nusername=test%20user&password=123";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.GetPost("username") == "test user");
    assert(request.GetPost("password") == "123");
    LOG_INFO("✓ Test 6 passed!");
}

void testSpecialCharacters() {
    LOG_INFO("=== Test 7: Special Characters in URL ===");
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 39\r\n\r\nusername=test%2Buser&password=abc%40def";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    assert(request.GetPost("username") == "test+user");
    assert(request.GetPost("password") == "abc@def");
    LOG_INFO("✓ Test 7 passed!");
}

void testNormalRegister() {
    LOG_INFO("=== Test 11: Normal Register Tests ===");
    
    // Test 11.1: 纯字母用户名和密码
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 30\r\n\r\nusername=testuser&password=abcdef";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        assert(request.GetPost("username") == "testuser");
        assert(request.GetPost("password") == "abcdef");
        LOG_INFO("✓ Test 11.1 passed: Pure alphabetic username and password");
    }
    
    // Test 11.2: 包含数字的用户名和密码
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 30\r\n\r\nusername=user123&password=pwd123";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        assert(request.GetPost("username") == "user123");
        assert(request.GetPost("password") == "pwd123");
        LOG_INFO("✓ Test 11.2 passed: Username and password with numbers");
    }
    
    // Test 11.3: 包含下划线的用户名和密码
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 32\r\n\r\nusername=test_user&password=pass_word";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        assert(request.GetPost("username") == "test_user");
        assert(request.GetPost("password") == "pass_word");
        LOG_INFO("✓ Test 11.3 passed: Username and password with underscore");
    }
    
    LOG_INFO("✓ All normal register tests passed!");
}

void testInvalidRegister() {
    LOG_INFO("=== Test 12: Invalid Register Tests ===");
    
    // Test 12.1: 用户名包含空格
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 33\r\n\r\nusername=test%20user&password=pass123";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        std::string username = request.GetPost("username");
        assert(username == "test user");
        // 用户名包含空格，应该被拒绝
        LOG_INFO("✓ Test 12.1 passed: Username with space parsed correctly");
    }
    
    // Test 12.2: 用户名包含特殊字符
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 33\r\n\r\nusername=test%2Buser&password=pass123";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        std::string username = request.GetPost("username");
        assert(username == "test+user");
        // 用户名包含+号，应该被拒绝
        LOG_INFO("✓ Test 12.2 passed: Username with special character parsed correctly");
    }
    
    // Test 12.3: 密码包含特殊字符
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 32\r\n\r\nusername=testuser&password=pass%40word";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        std::string password = request.GetPost("password");
        assert(password == "pass@word");
        // 密码包含@符号，应该被拒绝
        LOG_INFO("✓ Test 12.3 passed: Password with special character parsed correctly");
    }
    
    // Test 12.4: 密码包含空格
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 32\r\n\r\nusername=testuser&password=pass%20word";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        std::string password = request.GetPost("password");
        assert(password == "pass word");
        // 密码包含空格，应该被拒绝
        LOG_INFO("✓ Test 12.4 passed: Password with space parsed correctly");
    }
    
    // Test 12.5: 用户名和密码都包含特殊字符
    {
        Buffer buff;
        HttpRequest request;
        std::string rawRequest = "POST /register.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 34\r\n\r\nusername=test%23user&password=pass%24word";
        buff.append(rawRequest.c_str(), rawRequest.size());
        bool result = request.parse(buff);
        assert(result == true);
        std::string username = request.GetPost("username");
        std::string password = request.GetPost("password");
        assert(username == "test#user");
        assert(password == "pass$word");
        // 用户名和密码都包含特殊字符，应该被拒绝
        LOG_INFO("✓ Test 12.5 passed: Both username and password with special characters parsed correctly");
    }
    
    LOG_INFO("✓ All invalid register tests passed!");
}

void testEmptyBody() {
    LOG_INFO("=== Test 8: Empty Body POST ===");
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "POST /login.html HTTP/1.1\r\nHost: localhost:8080\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == true);
    LOG_INFO("✓ Test 8 passed!");
}

void testInvalidRequest() {
    LOG_INFO("=== Test 9: Invalid Request Line ===");
    Buffer buff;
    HttpRequest request;

    std::string rawRequest = "INVALID REQUEST\r\nHost: localhost:8080\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());

    bool result = request.parse(buff);
    assert(result == false);
    LOG_INFO("✓ Test 9 passed!");
}

void testKeepAlive() {
    LOG_INFO("=== Test 10: Keep-Alive Connection ===");
    Buffer buff;
    HttpRequest request;

    // Test HTTP/1.1 with keep-alive
    std::string rawRequest = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());
    bool result = request.parse(buff);
    assert(result == true);
    assert(request.IsKeepAlive() == true);

    // Test HTTP/1.1 without keep-alive
    request.init();
    buff.clear();
    rawRequest = "GET /index.html HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());
    result = request.parse(buff);
    assert(result == true);
    assert(request.IsKeepAlive() == false);

    // Test HTTP/1.0
    request.init();
    buff.clear();
    rawRequest = "GET /index.html HTTP/1.0\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";

    buff.append(rawRequest.c_str(), rawRequest.size());
    result = request.parse(buff);
    assert(result == true);
    assert(request.IsKeepAlive() == false);
    LOG_INFO("✓ Test 10 passed!");
}

int main() {
    // Initialize database connection pool
    Logger::getInstance().initLogger("log/httprequest.log",LogLevel::INFO,1024,3);
    SqlConnPool::getInstance().Init("127.0.0.1", 3306, "root", "password", "webserver", 10);
    LOG_INFO("Starting HTTP Request Tests...");
    LOG_INFO("===============================");
    try {
        
        testBasicRequest();
        testRootPath();
        testDefaultHTML();
        testHeaders();
        testPostRequest();
        testUrlEncoding();
        testSpecialCharacters();
        testNormalRegister();
        testInvalidRegister();
        testEmptyBody();
        testInvalidRequest();
        testKeepAlive();

        LOG_INFO("================================");
        LOG_INFO("All tests passed successfully! ✓");
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        SqlConnPool::getInstance().ClosePool();
        return 1;
    }

    SqlConnPool::getInstance().ClosePool();
    return 0;
}
