#include "libco/co_routine.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <queue>
#include <memory.h>
using namespace std;

int g_listenfd;

struct task_t
{
	int fd;
	stCoRoutine_t *co;
};

//处理读写任务的协程列表
queue<task_t*> g_routines;

//处理读写的协程函数，负责将空闲的协程放回列表
void* read_write_func(void *arg)
{
	co_enable_hook_sys();

	task_t* co = (task_t*) arg;
	char buf[1024*8];
	while(1)
	{
		//空闲协程
		if(co->fd == -1)
		{
			//先放回列表，yeild()等待下一次非空再回来
			g_routines.push(co);
			co_yield_ct();

			//下一次回来时，已经是非空的了
			continue;
		}

		int fd = co->fd;
		//循环读取和发送
		while(true)
		{
			//判断是否有数据过来（使用poll监听数据的到来）
			//poll会托管给ctx的epoll管理器，每次都需要重新注册
			struct pollfd pf = {0};
			pf.fd = fd;
			pf.events = (POLLIN|POLLERR|POLLHUP);
			poll(&pf,1,1000);
			//poll内部先yeild()，ctx的epoll管理器会监听，如果有事件到来
			//或者超时，resume将会返回

			//由于在accept时手动将fd设置为非阻塞，因此将会调用原生read
			//（不理解为什么要这么做）
			int ret = read(fd, buf, sizeof(buf));
			if(ret > 0)
			{
				write(fd, buf, ret);
				continue;
			}
			if(ret == -1&&errno == EAGAIN)
				continue;

			//对面已经关闭连接，此时应该将该协程置为空闲
			//（将co->fd置为-1，下一次外出层循环就会将其push进空闲列表）
			co->fd = -1;
			close(fd);
			break;
		}
	}
	return 0;
}

//监听并accept协程函数，负责将空闲协程取出并resume
void* accept_func(void *arg)
{
	co_enable_hook_sys();
	while(1)
	{
		//如果现在没有空闲的读写协程，那就先不要accept，先等一秒钟
		if(g_routines.empty())
		{
			poll(NULL, 0, 1000);
			//先yeild()，一秒后超时再被ctx管理器resume()
			continue;
		}

		struct sockaddr_in cliaddr;
		memset(&cliaddr, 0, sizeof(cliaddr));
		socklen_t len;
		
		//co_accept是个例外，它没有被hook，所以是原生的accept
		//在每次循环中都尝试accept，如果没有需要accept的，
		//就调用poll()等待连接到来或者1秒后再尝试
		int fd = accept(g_listenfd, (struct sockaddr*)&cliaddr,&len);
		
		if(fd < 0)
		{
			//没有需要accept的连接，调用poll来监视事件的到来
			struct pollfd pf = { 0 };
			pf.fd = g_listenfd;
			pf.events = (POLLIN|POLLERR|POLLHUP);
			poll(&pf, 1, 1000);
			//先yeild()，如果有连接到来或者超时再resume回来
			continue;
		}

		if(g_routines.empty())
		{
			//感觉这一步判断有点多余
			close( fd );
			continue;
		}

		//处理连接,注意这里的注释最终要删除
		//SetNonBlock( fd );		//设置为nonblock，因此不会走hook

		task_t* co = g_routines.front();
		co->fd = fd;
		g_routines.pop();
		co_resume(co->co);
	}
	return 0;
}


int main(int argc, char **argv)
{
	assert(argc == 2);

	//准备监听
	g_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	assert(g_listenfd >= 0);

	struct sockaddr_in servaddr;
	memset(&servaddr,0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = atoi(argv[1]);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int ret = bind(g_listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	assert(ret >= 0);

	//创建并启动若干个读写协程
	int cnt = 20;
	for(int i=0;i<cnt;i++)
	{
		task_t * task = (task_t*)calloc( 1,sizeof(task_t) );
		task->fd = -1;

		co_create( &(task->co),NULL, read_write_func,(void*)task );
		co_resume( task->co );
	}

	//创建并启动accept协程
	stCoRoutine_t *accept_co = NULL;
	co_create( &accept_co,NULL,accept_func,0 );
	co_resume( accept_co );

	co_eventloop( co_get_epoll_ct(),0,0 );


}



