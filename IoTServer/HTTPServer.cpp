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
    // �������HTTP/1.1��keep-alive �����ؿ��µ�socket��
    while (instance->Handler(client));
}

void* resize(void* src, size_t srcLen, size_t dstLen) {
    if (dstLen < srcLen) {
        // ��֧����С
        return nullptr;
    }
    else if (dstLen == srcLen) {
        /// �����ƶ�
        return src;
    }
    else {
        // ����
        char* mem = new char[dstLen];
        // д0
        memset(mem, 0, dstLen);
        // ����ԭ�����ڴ�
        memcpy(mem, src, srcLen);
        // ɾ��ԭ�е��ڴ�ռ�
        delete[] src;
        // �����µĿռ�
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

// ��ǰ�汾��֧���첽���� һ��ֻ�ܴ���һ������������ �ͻ�����һ�η���һ��HTTP����
// �ݲ�֧��մ��������첽���� ��ʵ����HTTP/1.0
// �ⲿ�ִ������� �����鿴���ϵķ���
bool HTTPServer::Handler(SOCKET client) {
    int recvLen = 0;
    int bufferCount = 1;
    int bufferSize = 1024;

    char* recvBuffer = new char[bufferSize];
    memset(recvBuffer, 0, bufferSize);

    char* headerEnd = nullptr;
    char* bodyData = nullptr; // ֻ�е�header��bufferװ���µ�ʱ�� �ؿ�һ��buffer������
    HTTPReqContext req;
    HTTPRespContext resp;

    while (true) {
        recvLen = recv(client, recvBuffer + (bufferCount - 1) * bufferSize, bufferSize, 0);
        if (recvLen < 0) {
            Log("��ȡSocket����");
            delete[] recvBuffer;
            return false;
        }
        else if (recvLen == 0) {
            // �Ͽ�����
            Log("�Ͽ�����");
            delete[] recvBuffer;
            return false;
        }
        // �ж��Ƿ����
        headerEnd = strstr(recvBuffer, "\r\n\r\n");
        if (headerEnd != nullptr) {
            // �ҵ���ͷ
            break;
        }
        // ���·���ռ�
        bufferCount++;
        int newSize = bufferCount * bufferSize;
        char* newBuffer = (char*)resize(recvBuffer, (bufferCount - 1) * bufferSize, newSize);
        recvBuffer = newBuffer;
    }
    // ��ʱ��������header��body��
    req.ParseHeader(recvBuffer);

    Log(req.method + string(" ") + req.url);

    // ����header����
    int headerLength = headerEnd - recvBuffer + 4;
    // �����ܹ������˶�������
    recvLen = recvLen + (bufferCount - 1) * bufferSize;
    // ����body��û��ȫ������
    if (recvLen != headerLength + req.contentLength) {
        // ����һ���ڴ������
        bodyData = new char[req.contentLength];
        int copyLen = recvLen - headerLength; // ����header�е�http������ ������
        memcpy(bodyData, recvBuffer + headerLength, copyLen);
        int bodyRecv = copyLen;
        // ���Ž���ѭ������recv
        while (bodyRecv != req.contentLength) {
            bodyRecv += recv(client, bodyData + bodyRecv, req.contentLength - bodyRecv, 0);
        }
    }
    else {
        // �Ѿ���������
        // ���ݵ�������ͷ���յ�
        req.bodyData = headerEnd + 4;
    }

    // �ҵ������� ���ô�����
    routerFunc serviceFunc = GetFunc(req.method, req.url);
    if (serviceFunc == nullptr) {
        // ����404
        resp.SetData(nullptr, 0, HTTP_NOT_FOUND);
        SendData(client, resp);
        goto end;
    }
    try {
        // ִ��ҵ�����
        serviceFunc(req, resp);
    }
    catch (...) {
        // ������
    }
    // ���ͻ�ȥ
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
    // ��װ���ĵ�һ��
    if (resp.status == HTTP_SUCCESS) {
        const char* header = "HTTP/1.1 200 OK\r\n";
        send(client, header, strlen(header), 0);
    }
    else if (resp.status == HTTP_NOT_FOUND) {
        const char* header = "HTTP/1.1 404 NOT FOUND\r\n";
        send(client, header, strlen(header), 0);
    }

    // ��װ����ͷ
    for (auto iter = resp.headerAttr->begin(); iter != resp.headerAttr->end(); iter++) {
        string line = iter->first + ": " + iter->second + "\r\n";
        send(client, line.c_str(), strlen(line.c_str()), 0);
    }

    // ���ͳ���
    string contentLength = "Content-Length: " + to_string(resp.contentLength) + "\r\n";
    send(client, contentLength.c_str(), strlen(contentLength.c_str()), 0);

    // ������������
    string type = "Content-Type: application/json\r\n";
    send(client, type.c_str(), type.length(), 0);

    send(client, "\r\n", 2, 0);
    // ���ͱ�����
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
    // ����TCP����
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

    // ����ip�Ͷ˿�
    char portStr[8];
    sprintf(portStr, "%d", m_port);
    ret = getaddrinfo(m_ip.c_str(), portStr, &hints, &result);
    if (ret != 0) {
        return false;
    }

    // �����׽���
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        return false;
    }

    // �󶨼�����Ϣ
    ret = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (ret == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return false;
    }
    freeaddrinfo(result);

    // ��������
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
        // ���������߳����
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
        // ������
        return 0;
    }
    memcpy(buf, src, lineEnd - src);

    return int(lineEnd - src);
}

bool HTTPReqContext::ParseHeader(char* header) {
    // ���ŵ�ǰ�������Ѿ����Զ���\r\n\r\n��
    char line[1024] = { 0 };
    int i = 0;

    // ��һ�еĶ�ȡ
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
        // ��ȡ��ֵ��
        header += lineLen + 2;
        memset(line, 0, sizeof(line));
        lineLen = GetLine(header, line, sizeof(line));
        if (!lineLen) {
            // ������
            break;
        }
        // ��ǰ����Ч
        char* colon = strchr(line, ':');
        if (colon == nullptr) {
            // �Ҳ���ð��
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
