#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <chrono>

std::string test()
{
    std::string read_in_buffer_ = "\r\nHost: djj\r\nAccept: text/html\r\n\r\n";
    /*!
       只检查每一行的格式。首部行的格式必须为"字段名：|空格|字段值|cr|lf"
       对首部字段是否正确，字段值是否正确均不做判断。
    */
    auto FormatCheck = [](std::string& target) -> bool
    {
        auto pos_colon = target.find(':');
        if(islower(target[0])                     //字段名的首字母必须大写
           ||pos_colon == std::string::npos        //字段名后必须有冒号
           || target[pos_colon+1] != ' '           //冒号后面必须紧跟一个空格
           || pos_colon+1 == target.size()-1)      //空格后面必须要有值
            return false;

        /*保存首部字段和对应的值*/
        std::string header = target.substr(0,pos_colon);
        std::string value = target.substr(pos_colon+2,target.size()-2-pos_colon);
        return true;
    };

    /*判断每个首部行的格式是否正确*/
    decltype(read_in_buffer_.size()) pos_cr = 0,old_pos = 0;
    while(true)
    {
        old_pos = pos_cr;
        pos_cr = read_in_buffer_.find("\r\n",pos_cr + 2);
        if(pos_cr == std::string::npos) return "again";   //接收到完整的首部行后才开始解析
        if(pos_cr == old_pos + 2)
        {
            read_in_buffer_ = read_in_buffer_.substr(pos_cr);
            break;//HeaderLinesParseState::kParseSuccess; //解析到空行了，则说明首部行格式没问题且数据完整
        }


        std::string header_line = read_in_buffer_.substr(old_pos+2,pos_cr-old_pos-2);
        if(!FormatCheck(header_line))
            return "fail";//HeaderLinesParseState::kParseError;
    }

   return "success";//HeaderLinesParseState::kParseAgain;
}
int main()
{
    time_t raw_time;
    struct tm* time_info;
    char time_buffer[30]{};
    time(&raw_time);
    time_info = gmtime(&raw_time);
    strftime(time_buffer, sizeof(time_buffer), "%a, %d %b %Y %H:%M:%S GMT", time_info);
    std::cout<<std::string(time_buffer) + "1111"<<std::endl;

    //std::cout<<test()<<std::endl;
}