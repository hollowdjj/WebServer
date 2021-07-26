#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <regex>

int main()
{
    //GET / HTTP/1.1\r\n
    std::string request_line("Content-Type: 1");
    try{
        std::regex r("^([A-Z]\\S*)\\:\\s(.+)$");
        std::smatch results;
        std::regex_match(request_line,results,r);
        std::cout<<"size: "<<results.size()<<" result: "<<results[1]<<std::endl;
    } catch(std::regex_error e)
    {
        std::cout<<e.what()<<"\ncode: "<<e.code()<<std::endl;
    };

    return 0;
}