#include "httprequest.h"
#include <algorithm>

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML {
    "/login", "/register", "/index",
    "/welcome", "/video", "/picture" 
};


const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1},  
};

void HttpRequest::init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        // header_用const修饰，不支持使用[]运算符，只能使用find函数
        return header_.find("Connection")->second == "keep-alive" and version_ == "1.1";
    }
    return false;
}
/**
 * @brief 解析HTTP请求的函数，基于状态机逐行解析请求行、请求头和请求体
 * @details HTTP请求的标准格式如下：
 *          请求行（GET / HTTP/1.1\r\n）
 *          请求头1（Host: localhost\r\n）
 *          请求头2（Content-Length: 0\r\n）
 *          空行（\r\n）  → 请求头结束标志
 *          请求体（可选，POST 请求才有）
 * @param buff 输入的缓冲区对象，包含待解析的HTTP原始字节数据；要求缓冲区已初始化且有可读数据
 * @return true 表示解析流程正常执行（包括“数据不足需等待”“解析完成”等场景）；false 表示解析出错（如请求行格式非法、缓冲区为空）
 */
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n"; // Http请求行的结束标志
    if(buff.readable_size() <= 0) return false;
    while(buff.readable_size() and state_ != FINISH) {
        const char* line_end = std::search(buff.peek(), buff.begin_write_const( ), CRLF, CRLF + 2);
        std::string line(buff.peek(), line_end);
        switch(state_) {
            case REQUEST_LINE:
                if(!ParseRequestLine_(line))  return false;
                ParsePath_();
                break;
            case HEADERS:
                ParseHeader_(line);
                if(buff.readable_size() <= 2) state_ = FINISH;
                break;
            case BODY:
                ParseBody_(line);
                break;
            default:
                break;
        }
        if(line_end == buff.begin_write_const()) break;
        buff.retrieve_until(line_end + 2);
    }
    LOG_DEBUG("[{}], [{}], [{}]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}

bool HttpRequest::ParseRequestLine_(const std::string line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch submatch;
    if(std::regex_match(line, submatch, pattern)) {
        method_ = submatch[1];
        path_ = submatch[2];
        version_ = submatch[3];
        state_ = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error : %s", line.c_str());
    return false;
}

void HttpRequest::ParsePath_() {
    if(path_ == "/") path_ = "/index.html";
    else {
        for(auto &item : DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}   

void HttpRequest::ParseHeader_(const std::string& line) {
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch submatch;
    if(std::regex_match(line, submatch, pattern)) {
        header_[submatch[1]] = submatch[2];
    }
    else state_ = BODY;
}       

void HttpRequest::ParseBody_(const std::string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body : %s, len : %d", line.c_str(), line.size());
}

/**
 * @brief 解析POST请求的函数
 * 该函数处理POST请求，特别是Content-Type为application/x-www-form-urlencoded的请求
 * 它会解析表单数据，并根据请求路径进行相应的用户验证
 */
void HttpRequest::ParsePost_() {
    // 检查请求方法是否为POST且Content-Type是否为表单编码类型
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        // 解码URL编码的POST数据
        ParseFromUrlencoded_();
        // 检查当前路径是否在预定义的HTML标签映射中
        if(DEFAULT_HTML_TAG.count(path_)) {
            // 获取对应的标签值
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            // 根据标签值判断是登录还是注册操作
            if(tag == 0 or tag == 1) {
                // 设置是否为登录操作的标志
                bool isLogin = (tag == 1);
                // 验证用户名和密码
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    // 验证成功，重定向到欢迎页面
                    path_ = "/welcome.html";
                } 
                else {
                    // 验证失败，重定向到错误页面
                    path_ = "/error.html";
                }
            }
        }
    }   
}

/**
 * 将十六进制字符转换为对应的整数值
 * @param ch 需要转换的字符
 * @return 返回转换后的整数值，如果不是有效的十六进制字符则返回字符本身
 */
int HttpRequest::ConverHex(char ch) {
    // 如果字符是大写字母 A-F，则转换为对应的整数值 (10-15)
    if(ch >= 'A' && ch <= 'F') return ch -'A' + 10;
    // 如果字符是小写字母 a-f，则转换为对应的整数值 (10-15)
    if(ch >= 'a' && ch <= 'f') return ch -'a' + 10;
    // 如果字符不是有效的十六进制字符，则返回字符本身
    return ch;
}

/**
 * @brief 从URL编码的请求体中解析键值对
 * 该函数处理URL编码格式的请求体，将其转换为键值对并存入post_映射中
 * 支持处理普通字符、加号空格、百分号编码以及特殊字符分隔符
 */
void HttpRequest::ParseFromUrlencoded_() {
    if (body_.empty()) { return; }  // 如果请求体为空，直接返回
    std::string key, value;  // 用于存储当前解析的键和值
    int n = body_.size();    // 获取请求体长度
    int i = 0, j = 0;        // i为当前遍历位置，j为当前键值对的起始位置
    while (i < n) {
        char ch = body_[i];
        switch (ch) {
            case '=':  // 遇到等号，表示键的结束
                if(j < i) key = body_.substr(j, i - j);  // 提取键
                j = i + 1;  // 更新值的起始位置
                i++;
                break;
            
            case '+':  // 加号表示空格
                // 不修改原 body_，而是在拼接 value 时处理
                value += ' ';  // 将加号转换为空格
                j = i + 1;
                i++;
                break;
            
            case '%': {  // 加花括号避免变量重定义
                // 边界校验：确保 % 后有至少两位字符
                if(i + 2 >= n) {
                    i++;
                    break;
                }
                int high = ConverHex(body_[i + 1]);
                int low = ConverHex(body_[i + 2]);
                // 校验十六进制字符合法性
                if (high == -1 or low == -1) {
                    i += 3;
                    j = i;
                    break;
                }
                // 计算正确的 ASCII 字符
                char decoded = high * 16 + low;
                value += decoded;
                i += 3;  // 跳过 %xx 三位
                j = i;
                break;
            }
            case '&':
                // 截取 value，避免空 value
                if (j < i and !key.empty()) {
                    value += body_.substr(j, i - j);
                    post_[key] = value;
                    LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                    // 重置 key/value，准备下一个键值对
                    key.clear();
                    value.clear();
                }
                j = i + 1;
                i++;
                break;
            
            default:
                // 普通字符直接拼入 value
                if(!key.empty())  // 只有解析完 key 后才拼接 value
                    value += ch;
                i++;
                break;
        }
    }
    // 处理最后一个键值对
    if (!key.empty() && j < n) {
        value += body_.substr(j, n - j);
        post_[key] = value;
    }
}
std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) return post_.find(key)->second;
    return "";
}






