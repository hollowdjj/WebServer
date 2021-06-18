#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>

/*！
@Author: DJJ
@Description: 服务器测试代码，使用非阻塞connect
@Date: 2021/6/18 上午11:13
*/

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}
int main(int argc,char* argv[])
{
    if(argc <=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }

}



