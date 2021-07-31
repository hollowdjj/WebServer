/*！
@Author: DJJ
@Date: 2021/7/31 下午10:53
*/
#ifndef WEBSERVER_NONCOPYABLE_H
#define WEBSERVER_NONCOPYABLE_H

/*!
@brief nocopyable基类。
*/
class NonCopyable{
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

#endif //WEBSERVER_NONCOPYABLE_H
