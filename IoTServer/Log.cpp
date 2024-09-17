#include "Log.h"
#include <iostream>

void Log(string log) {
    time_t nowtime;
    time(&nowtime); //获取1970年1月1日0点0分0秒到现在经过的秒数
    tm* p = localtime(&nowtime); //将秒数转换为本地时间,年从1900算起,需要+1900,月为0-11,所以要+1
    char time[128] = { 0 };
    sprintf(time, "%04d:%02d:%02d %02d:%02d:%02d", p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);

    cout << '[' << time << '\]'<< ' ';
    cout << log << endl;
}