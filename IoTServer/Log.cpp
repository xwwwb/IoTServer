#include "Log.h"
#include <iostream>

void Log(string log) {
    time_t nowtime;
    time(&nowtime); //��ȡ1970��1��1��0��0��0�뵽���ھ���������
    tm* p = localtime(&nowtime); //������ת��Ϊ����ʱ��,���1900����,��Ҫ+1900,��Ϊ0-11,����Ҫ+1
    char time[128] = { 0 };
    sprintf(time, "%04d:%02d:%02d %02d:%02d:%02d", p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);

    cout << '[' << time << '\]'<< ' ';
    cout << log << endl;
}