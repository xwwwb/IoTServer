#pragma once
#include <string>
#include <iostream>
#include <iomanip>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

class HTTPReqContext
{
public:
    HTTPReqContext() {
        headerAttr = new map<string, string>();
        memset(method, 0, 32);
        memset(url, 0, 128);
        memset(httpVer, 0, 32);
        bodyData = nullptr;
        contentLength = 0;
    }
    ~HTTPReqContext() {
        delete headerAttr;
        // bodyData��¼�������ⲿ�������ڴ��ַ ���ﲻ�������
        if (bodyData != nullptr) {
            bodyData = nullptr;
        }
    }
    // ��ŵ���header
    map<string, string>* headerAttr;
    char method[32]; // HTTP����
    char url[128]; // HTTPUrl
    char httpVer[32];
    char* bodyData; // ָ���������ָ��
    long long contentLength;

    // ����ͷ�ķ���
    bool ParseHeader(char* header);
    // ����\r\n�õ���
    int GetLine(char* src, char* buf, size_t bufLen);
    // ��ӡ��ǰReqContext
    inline void PrintContext() const {
        const int keyWidth = 30; // ����Key�Ŀ��
        const int valueWidth = 40; // Value�Ŀ��

        cout << left << setw(keyWidth) << "Method:" << method << endl;
        cout << left << setw(keyWidth) << "URL:" << url << endl;
        cout << left << setw(keyWidth) << "HTTP Version:" << httpVer << "\n" << endl;
        cout << left << setw(keyWidth) << "Headers:" << endl;

        for (const auto& entry : *headerAttr) {
            string key = entry.first;
            string value = entry.second;

            cout << left << setw(keyWidth) << key << left << setw(valueWidth) << value << endl;
        }
        cout << "\nBody Data: " << endl;
        for (int i = 0; i < contentLength; i++) {
            cout << bodyData[i];
        }
        cout << endl;
    }
};

enum HTTPStatus {
    HTTP_SUCCESS = 200,
    HTTP_NOT_FOUND = 404
};

class HTTPRespContext {
public:
    HTTPRespContext() {
        headerAttr = new map<string, string>();
        bodyData = nullptr;
        contentLength = 0;
        status = HTTP_SUCCESS;

    }
    ~HTTPRespContext() {
        delete headerAttr;
        if (bodyData != nullptr) {
            delete[] bodyData;
            bodyData = nullptr;
        }
        contentLength = 0;
        status = HTTP_SUCCESS;
    }
    void AddHeader(string key, string value);
    void SetData(char* data, long long dataLength, HTTPStatus status);
    map<string, string>* headerAttr;
    char* bodyData;
    long long contentLength;
    HTTPStatus status;
};

typedef void (*routerFunc)(HTTPReqContext&, HTTPRespContext&);

class HTTPServer
{
public:
    HTTPServer() { m_ip = ""; m_port = 0; };
    HTTPServer(string ip, int port);

    bool StartServer();
    bool Handler(SOCKET client);
    bool AddRouter(string method, string url, routerFunc func);
    bool SendData(SOCKET client, HTTPRespContext& resp);
    routerFunc GetFunc(string method, string url);
private:
    string m_ip;
    int m_port;
    map<string, routerFunc> router;
};