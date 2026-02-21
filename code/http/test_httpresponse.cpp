
#include "httpresponse.h"
#include "../buffer/buffer.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>

// 辅助函数：创建测试文件
void createTestFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (file.is_open()) {
        file << content;
        file.close();
    }
}

// 辅助函数：创建测试目录和文件
void setupTestResources(const std::string& testDir) {
    // 创建测试目录
    std::filesystem::create_directories(testDir);

    // 创建测试HTML文件
    createTestFile(testDir + "/index.html", "<html><body>Index Page</body></html>");
    createTestFile(testDir + "/test.html", "<html><body>Test Page</body></html>");
    createTestFile(testDir + "/style.css", "body { color: red; }");
    createTestFile(testDir + "/script.js", "console.log('test');");
    createTestFile(testDir + "/image.jpg", "fake image content");
    createTestFile(testDir + "/data.txt", "plain text content");

    // 创建错误页面
    createTestFile(testDir + "/400.html", "<html><body>400 Bad Request</body></html>");
    createTestFile(testDir + "/403.html", "<html><body>403 Forbidden</body></html>");
    createTestFile(testDir + "/404.html", "<html><body>404 Not Found</body></html>");
    createTestFile(testDir + "/500.html", "<html><body>500 Internal Server Error</body></html>");
}

// 辅助函数：清理测试资源
void cleanupTestResources(const std::string& testDir) {
    std::filesystem::remove_all(testDir);
}

// 测试1: 基本的HTML文件响应
void testBasicHtmlResponse() {
    LOG_INFO("=== Test 1: Basic HTML Response ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/index.html";

    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);

    std::string responseStr(buff.peek(), buff.readable_size());

    // 检查响应状态行
    assert(responseStr.find("HTTP/1.1 200 OK") != std::string::npos);
    // 检查Content-Type
    assert(responseStr.find("Content-Type: text/html") != std::string::npos);
    // 检查Connection头
    assert(responseStr.find("Connection: close") != std::string::npos);
    // 检查响应体
    assert(responseStr.find("Index Page") != std::string::npos);
    // 检查Content-Length
    assert(responseStr.find("Content-Length:") != std::string::npos);

    LOG_INFO("✓ Test 1 passed!");
    cleanupTestResources(testDir);
}

// 测试2: Keep-Alive连接
void testKeepAliveResponse() {
    LOG_INFO("=== Test 2: Keep-Alive Connection ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/test.html";

    response.Init(testDir, path, true, -1);
    response.MakeResponse(buff);

    std::string responseStr(buff.peek(), buff.readable_size());

    // 检查Keep-Alive头
    assert(responseStr.find("Connection: keep-alive") != std::string::npos);
    assert(responseStr.find("Keep-Alive: max=6, timeout=120") != std::string::npos);

    LOG_INFO("✓ Test 2 passed!");
    cleanupTestResources(testDir);
}

// 测试3: 不同文件类型的Content-Type
void testFileTypes() {
    LOG_INFO("=== Test 3: Different File Types ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 测试CSS文件
    {
        HttpResponse response;
        Buffer buff;
        std::string path = "/style.css";

        response.Init(testDir, path, false, -1);
        response.MakeResponse(buff);

        std::string responseStr(buff.peek(), buff.readable_size());
        assert(responseStr.find("Content-Type: text/css") != std::string::npos);
        LOG_INFO("✓ CSS file type test passed");
    }

    // 测试JavaScript文件
    {
        HttpResponse response;
        Buffer buff;
        std::string path = "/script.js";

        response.Init(testDir, path, false, -1);
        response.MakeResponse(buff);

        std::string responseStr(buff.peek(), buff.readable_size());
        assert(responseStr.find("Content-Type: text/javascript") != std::string::npos);
        LOG_INFO("✓ JavaScript file type test passed");
    }

    // 测试JPEG文件
    {
        HttpResponse response;
        Buffer buff;
        std::string path = "/image.jpg";

        response.Init(testDir, path, false, -1);
        response.MakeResponse(buff);

        std::string responseStr(buff.peek(), buff.readable_size());
        assert(responseStr.find("Content-Type: image/jpeg") != std::string::npos);
        LOG_INFO("✓ JPEG file type test passed");
    }

    // 测试纯文本文件
    {
        HttpResponse response;
        Buffer buff;
        std::string path = "/data.txt";

        response.Init(testDir, path, false, -1);
        response.MakeResponse(buff);

        std::string responseStr(buff.peek(), buff.readable_size());
        assert(responseStr.find("Content-Type: text/plain") != std::string::npos);
        LOG_INFO("✓ Plain text file type test passed");
    }

    LOG_INFO("✓ Test 3 passed!");
    cleanupTestResources(testDir);
}

// 测试4: 404错误响应
void testNotFoundResponse() {
    LOG_INFO("=== Test 4: 404 Not Found ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/nonexistent.html";

    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);

    std::string responseStr(buff.peek(), buff.readable_size());

    // 检查404状态码
    assert(responseStr.find("HTTP/1.1 404 Not Found") != std::string::npos);
    // 检查错误页面内容
    assert(responseStr.find("404 Not Found") != std::string::npos);

    LOG_INFO("✓ Test 4 passed!");
    cleanupTestResources(testDir);
}

// 测试5: 403错误响应（不可读文件）
void testForbiddenResponse() {
    LOG_INFO("=== Test 5: 403 Forbidden ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 创建一个不可读的文件
    std::string filePath = testDir + "/forbidden.html";
    createTestFile(filePath, "<html><body>Forbidden</body></html>");
    chmod(filePath.c_str(), 0000); // 设置为不可读

    HttpResponse response;
    Buffer buff;
    std::string path = "/forbidden.html";

    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);

    std::string responseStr(buff.peek(), buff.readable_size());

    // 检查403状态码
    assert(responseStr.find("HTTP/1.1 403 Forbidden") != std::string::npos);

    // 恢复文件权限以便删除
    chmod(filePath.c_str(), 0644);

    LOG_INFO("✓ Test 5 passed!");
    cleanupTestResources(testDir);
}

// 测试6: 自定义错误码响应
void testCustomErrorResponse() {
    LOG_INFO("=== Test 6: Custom Error Response ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 测试400错误
    {
        HttpResponse response;
        Buffer buff;
        std::string path = "/index.html";

        response.Init(testDir, path, false, 400);
        response.MakeResponse(buff);

        std::string responseStr(buff.peek(), buff.readable_size());
        assert(responseStr.find("HTTP/1.1 400 Bad Request") != std::string::npos);
        assert(responseStr.find("400 Bad Request") != std::string::npos);
        LOG_INFO("✓ 400 error test passed");
    }

    // 测试500错误
    {
        HttpResponse response;
        Buffer buff;
        std::string path = "/index.html";

        response.Init(testDir, path, false, 500);
        response.MakeResponse(buff);

        std::string responseStr(buff.peek(), buff.readable_size());
        assert(responseStr.find("HTTP/1.1 500 Internal Server Error") != std::string::npos);
        assert(responseStr.find("500 Internal Server Error") != std::string::npos);
        LOG_INFO("✓ 500 error test passed");
    }

    LOG_INFO("✓ Test 6 passed!");
    cleanupTestResources(testDir);
}

// 测试7: 文件映射和长度
void testFileMapping() {
    LOG_INFO("=== Test 7: File Mapping ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/test.html";

    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);

    // 检查文件指针
    char* filePtr = response.GetFile();
    assert(filePtr != nullptr);

    // 检查文件长度
    size_t fileLen = response.FileLen();
    assert(fileLen > 0);

    // 验证文件内容
    std::string fileContent(filePtr, fileLen);
    assert(fileContent.find("Test Page") != std::string::npos);

    LOG_INFO("✓ Test 7 passed!");
    cleanupTestResources(testDir);
}

// 测试8: 文件解除映射
void testFileUnmapping() {
    LOG_INFO("=== Test 8: File Unmapping ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/test.html";

    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);

    // 确保文件已映射
    assert(response.GetFile() != nullptr);

    // 解除文件映射
    response.UnmapFile();

    // 确保文件指针已清空
    assert(response.GetFile() == nullptr);

    LOG_INFO("✓ Test 8 passed!");
    cleanupTestResources(testDir);
}

// 测试9: 错误内容生成
void testErrorContent() {
    LOG_INFO("=== Test 9: Error Content Generation ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/index.html";

    response.Init(testDir, path, false, 404);
    response.ErrorContent(buff, "Custom Error Message");

    std::string responseStr(buff.peek(), buff.readable_size());

    // 检查错误内容
    assert(responseStr.find("404 : Not Found") != std::string::npos);
    assert(responseStr.find("Custom Error Message") != std::string::npos);
    assert(responseStr.find("<html>") != std::string::npos);
    assert(responseStr.find("WebServer") != std::string::npos);

    LOG_INFO("✓ Test 9 passed!");
    cleanupTestResources(testDir);
}

// 测试10: 状态码获取
void testCodeGetter() {
    LOG_INFO("=== Test 10: Code Getter ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 测试200状态码
    {
        HttpResponse response;
        std::string path = "/index.html";

        response.Init(testDir, path, false, -1);
        assert(response.Code() == -1); // 初始状态

        Buffer buff;
        response.MakeResponse(buff);
        assert(response.Code() == 200); // 成功响应
        LOG_INFO("✓ 200 code test passed");
    }

    // 测试404状态码
    {
        HttpResponse response;
        std::string path = "/nonexistent.html";

        response.Init(testDir, path, false, -1);
        assert(response.Code() == -1); // 初始状态

        Buffer buff;
        response.MakeResponse(buff);
        assert(response.Code() == 404); // 文件不存在
        LOG_INFO("✓ 404 code test passed");
    }

    // 测试自定义状态码
    {
        HttpResponse response;
        std::string path = "/index.html";

        response.Init(testDir, path, false, 500);
        assert(response.Code() == 500); // 自定义状态码
        LOG_INFO("✓ Custom code test passed");
    }

    LOG_INFO("✓ Test 10 passed!");
    cleanupTestResources(testDir);
}

// 测试11: 重新初始化响应
void testReinitResponse() {
    LOG_INFO("=== Test 11: Reinitialize Response ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    HttpResponse response;
    Buffer buff;
    std::string path = "/index.html";

    // 第一次初始化和响应
    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);
    assert(response.Code() == 200);

    // 重新初始化
    buff.clear();
    path = "/test.html";
    response.Init(testDir, path, true, -1);
    response.MakeResponse(buff);
    assert(response.Code() == 200);

    std::string responseStr(buff.peek(), buff.readable_size());
    assert(responseStr.find("Test Page") != std::string::npos);
    assert(responseStr.find("Connection: keep-alive") != std::string::npos);

    LOG_INFO("✓ Test 11 passed!");
    cleanupTestResources(testDir);
}

// 测试12: 无后缀文件类型
void testNoExtensionFile() {
    LOG_INFO("=== Test 12: File Without Extension ===");
    std::string testDir = "test_resources";
    setupTestResources(testDir);

    // 创建一个没有后缀的文件
    createTestFile(testDir + "/README", "This is a README file");

    HttpResponse response;
    Buffer buff;
    std::string path = "/README";

    response.Init(testDir, path, false, -1);
    response.MakeResponse(buff);

    std::string responseStr(buff.peek(), buff.readable_size());

    // 检查默认Content-Type
    assert(responseStr.find("Content-Type: text/plain") != std::string::npos);
    // 检查文件内容
    assert(responseStr.find("This is a README file") != std::string::npos);

    LOG_INFO("✓ Test 12 passed!");
    cleanupTestResources(testDir);
}

int main() {
    // 初始化日志系统
    Logger::getInstance().initLogger("log/httpresponse.log", LogLevel::INFO, 1024, 3);

    LOG_INFO("Starting HTTP Response Tests...");
    LOG_INFO("===============================");

    try {
        testBasicHtmlResponse();
        testKeepAliveResponse();
        testFileTypes();
        testNotFoundResponse();
        testForbiddenResponse();
        testCustomErrorResponse();
        testFileMapping();
        testFileUnmapping();
        testErrorContent();
        testCodeGetter();
        testReinitResponse();
        testNoExtensionFile();

        LOG_INFO("================================");
        LOG_INFO("All tests passed successfully! ✓");
    } catch (const std::exception& e) {
        LOG_ERROR("Test failed with exception: {}", e.what());
        Logger::getInstance().shutdown();
        return 1;
    }

    Logger::getInstance().shutdown();
    return 0;
}
