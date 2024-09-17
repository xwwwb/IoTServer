#include <iostream>
#include "Log.h"
#include "HTTPServer.h"

using namespace std;

void DeviceInfo(HTTPReqContext& require, HTTPRespContext& response) {
    const char* msg = "{ \"msg\":\"Hello Server\" }";
    response.SetData((char*)msg, strlen(msg), HTTP_SUCCESS);
}

int main(int argc, char* argv[]) {
    HTTPServer httpServer("127.0.0.1", 8080);
    httpServer.AddRouter("POST", "/", DeviceInfo);
    httpServer.AddRouter("GET", "/", DeviceInfo);
    httpServer.StartServer();

}