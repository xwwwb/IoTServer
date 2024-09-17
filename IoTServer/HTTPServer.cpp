#include <thread>
#include <cstring>
#include <algorithm>
#include "HTTPServer.h"
#include "Log.h"

#pragma comment (lib, "Ws2_32.lib")

static HTTPServer* instance = nullptr;

HTTPServer::HTTPServer(string ip, int port) :m_ip(ip), m_port(port) {
    instance = this;
}

void AcceptHandler(SOCKET client) {
    // 这里采用HTTP/1.1的keep-alive 不再重开新的socket了
    while (instance->Handler(client));
}

void* resize(void* src, size_t srcLen, size_t dstLen) {
    if (dstLen < srcLen) {
        // 不支持缩小
        return nullptr;
    }
    else if (dstLen == srcLen) {
        /// 不做移动
        return src;
    }
    else {
        // 扩大
        char* mem = new char[dstLen];
        // 写0
        memset(mem, 0, dstLen);
        // 拷贝原来的内存
        memcpy(mem, src, srcLen);
        // 删除原有的内存空间
        delete[] src;
        // 返回新的空间
        return (void*)mem;
    }
}

void trimString(std::string& str)
{
    size_t s = str.find_first_not_of(" ");
    size_t e = str.find_last_not_of(" ");
    str = str.substr(s, e - s + 1);
    return;
}

// 当前版本不支持异步请求 一次只能处理一个完整的请求 客户端请一次发送一条HTTP请求
// 暂不支持沾包处理和异步请求 其实就是HTTP/1.0
// 这部分存在疑问 回来查看网上的范例
bool HTTPServer::Handler(SOCKET client) {
    int recvLen = 0;
    int bufferCount = 1;
    int bufferSize = 1024;

    char* recvBuffer = new char[bufferSize];
    memset(recvBuffer, 0, bufferSize);

    char* headerEnd = nullptr;
    char* bodyData = nullptr; // 只有当header的buffer装不下的时候 重开一个buffer存数据
    HTTPReqContext req;
    HTTPRespContext resp;

    while (true) {
        recvLen = recv(client, recvBuffer + (bufferCount - 1) * bufferSize, bufferSize, 0);
        if (recvLen < 0) {
            Log("读取Socket出错");
            delete[] recvBuffer;
            return false;
        }
        else if (recvLen == 0) {
            // 断开链接
            Log("断开链接");
            delete[] recvBuffer;
            return false;
        }
        // 判断是否结束
        headerEnd = strstr(recvBuffer, "\r\n\r\n");
        if (headerEnd != nullptr) {
            // 找到了头
            break;
        }
        // 重新分配空间
        bufferCount++;
        int newSize = bufferCount * bufferSize;
        char* newBuffer = (char*)resize(recvBuffer, (bufferCount - 1) * bufferSize, newSize);
        recvBuffer = newBuffer;
    }
    // 此时可以区分header和body了
    req.ParseHeader(recvBuffer);

    Log(req.method + string(" ") + req.url);

    // 计算header长度
    int headerLength = headerEnd - recvBuffer + 4;
    // 计算总共接受了多少数据
    recvLen = recvLen + (bufferCount - 1) * bufferSize;
    // 计算body有没有全部进来
    if (recvLen != headerLength + req.contentLength) {
        // 开辟一个内存存数据
        bodyData = new char[req.contentLength];
        int copyLen = recvLen - headerLength; // 存在header中的http请求体 不完整
        memcpy(bodyData, recvBuffer + headerLength, copyLen);
        int bodyRecv = copyLen;
        // 接着进行循环调用recv
        while (bodyRecv != req.contentLength) {
            bodyRecv += recv(client, bodyData + bodyRecv, req.contentLength - bodyRecv, 0);
        }
    }
    else {
        // 已经接受完了
        // 数据的起点就是头的终点
        req.bodyData = headerEnd + 4;
    }

    // 找到处理函数 调用处理函数
    routerFunc serviceFunc = GetFunc(req.method, req.url);
    if (serviceFunc == nullptr) {
        // 设置404
        resp.SetData(nullptr, 0, HTTP_NOT_FOUND);
        SendData(client, resp);
        goto end;
    }
    try {
        // 执行业务代码
        serviceFunc(req, resp);
    }
    catch (...) {
        // 错误处理
    }
    // 发送回去
    SendData(client, resp);

end:
    delete[] recvBuffer;
    if (bodyData != nullptr) {
        delete[] bodyData;
    }
    return true;
}

bool HTTPServer::AddRouter(string method, string url, routerFunc func)
{
    string key = method + '@' + url;
    transform(key.begin(), key.end(), key.begin(), tolower);
    router.insert(pair<string, routerFunc>(key, func));
    return true;
}

bool HTTPServer::SendData(SOCKET client, HTTPRespContext& resp)
{
    // 组装报文第一行
    if (resp.status == HTTP_SUCCESS) {
        const char* header = "HTTP/1.1 200 OK\r\n";
        send(client, header, strlen(header), 0);
    }
    else if (resp.status == HTTP_NOT_FOUND) {
        const char* header = "HTTP/1.1 404 NOT FOUND\r\n";
        send(client, header, strlen(header), 0);
    }

    // 组装报文头
    for (auto iter = resp.headerAttr->begin(); iter != resp.headerAttr->end(); iter++) {
        string line = iter->first + ": " + iter->second + "\r\n";
        send(client, line.c_str(), strlen(line.c_str()), 0);
    }

    // 发送长度
    string contentLength = "Content-Length: " + to_string(resp.contentLength) + "\r\n";
    send(client, contentLength.c_str(), strlen(contentLength.c_str()), 0);

    // 发送数据类型
    string type = "Content-Type: application/json\r\n";
    send(client, type.c_str(), type.length(), 0);

    send(client, "\r\n", 2, 0);
    // 发送报文体
    if (resp.bodyData != nullptr && resp.contentLength != 0) {
        send(client, resp.bodyData, resp.contentLength, 0);
    }
    return true;
}


routerFunc HTTPServer::GetFunc(string method, string url)
{
    string key = method + '@' + url;
    transform(key.begin(), key.end(), key.begin(), tolower);
    auto iter = router.find(key);
    if (iter != router.end()) {
        return iter->second;
    }
    return nullptr;
}

bool HTTPServer::StartServer() {
    if (m_ip.empty() || m_port == 0) {

        return false;
    }
    // 启动TCP监听
    WSADATA wsaData;
    int ret;
    SOCKET ListenSocket = INVALID_SOCKET;

    struct addrinfo* result = NULL;
    struct addrinfo hints;
    ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        return false;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // 处理ip和端口
    char portStr[8];
    sprintf(portStr, "%d", m_port);
    ret = getaddrinfo(m_ip.c_str(), portStr, &hints, &result);
    if (ret != 0) {
        return false;
    }

    // 开启套接字
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        return false;
    }

    // 绑定监听信息
    ret = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (ret == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return false;
    }
    freeaddrinfo(result);

    // 启动监听
    ret = listen(ListenSocket, SOMAXCONN);

    if (ret == SOCKET_ERROR) {
        closesocket(ListenSocket);
        WSACleanup();
        return false;
    }
    Log("Server Start Success");

    while (true) {
        SOCKET client = accept(ListenSocket, NULL, NULL);
        if (client == INVALID_SOCKET) {
            continue;
        }
        // 交给独立线程完成
        std::thread handler(AcceptHandler, client);
        handler.detach();
    }
}

int HTTPReqContext::GetLine(char* src, char* buf, size_t bufLen) {
    char* lineEnd = strstr(src, "\r\n");
    if (lineEnd == nullptr) {
        return 0;
    }
    if ((unsigned long long)(lineEnd - src) > bufLen) {
        // 不够存
        return 0;
    }
    memcpy(buf, src, lineEnd - src);

    return int(lineEnd - src);
}

bool HTTPReqContext::ParseHeader(char* header) {
    // 相信当前缓冲区已经可以读到\r\n\r\n了
    char line[1024] = { 0 };
    int i = 0;

    // 第一行的读取
    int lineLen = GetLine(header, line, sizeof(line));
    if (!lineLen) {
        return false;
    }
    char* temp = header;
    while (*temp != ' ' && i < sizeof(line) - 1) {
        method[i] = *temp++;
        i++;
    }

    while (*temp == ' ' || *temp == '\t') temp++;

    i = 0;
    while (*temp != ' ' && *temp != '\r' && *temp != '\n' && i < sizeof(line) - 1) {
        url[i] = *temp++;
        i++;
    }

    while (*temp == ' ' || *temp == '\t') temp++;

    i = 0;
    while (*temp != ' ' && *temp != '\r' && *temp != '\n' && i < sizeof(line) - 1) {
        httpVer[i] = *temp++;
        i++;
    }

    while (true) {
        // 读取键值对
        header += lineLen + 2;
        memset(line, 0, sizeof(line));
        lineLen = GetLine(header, line, sizeof(line));
        if (!lineLen) {
            // 结束了
            break;
        }
        // 当前行有效
        char* colon = strchr(line, ':');
        if (colon == nullptr) {
            // 找不到冒号
            return false;
        }
        string key(line, colon - line);
        string value(colon + 1);
        trimString(key);
        trimString(value);
        headerAttr->insert(pair<string, string>(key, value));
    }

    auto iter_1 = headerAttr->find("Content-Length");
    auto iter_2 = headerAttr->find("content-length");
    if (iter_1 != headerAttr->end()) {
        contentLength = stoll(iter_1->second);
    }
    else if (iter_2 != headerAttr->end()) {
        contentLength = stoll(iter_2->second);
    }
    else {
        contentLength = 0;
    }

    return true;
}

void HTTPRespContext::AddHeader(string key, string value)
{
    headerAttr->insert(pair<string, string>(key, value));
}


void HTTPRespContext::SetData(char* data, long long dataLength, HTTPStatus status)
{
    bodyData = new char[dataLength];
    memcpy(bodyData, data, dataLength);
    contentLength = dataLength;
    this->status = status;
}
