/*
* libco的核心调度，在此处处理所有协程和事件
* 
* @param: ctx  epoll管理器
* @param: pfn  每轮事件循环后都会调用该函数
* @param: arg  pfn的参数
* 
*/

/*
 * stCoEpoll_t（ctx）包括的内容：
 *
 * 1.epollfd
 * 2.pTimeout:指针，指向一个时间轮定时器结构体(该结构体存放了注册的超时事件)
 * 3.pstTimeoutList:一个链表头，该链表用于临时存放已超时事件
 * 4.pstActiveList:一个链表头，最终会存放就绪事件和已超时事件
 * 5.result: 对 epoll_wait() 第二个参数的封装，即epoll_wait()得到的结果
 *
 */
void co_eventloop(stCoEpoll_t* ctx, pfn_co_eventloop_t pfn, void* arg)
{
	//result用于储存epoll_wait()的结果
	co_epoll_res* result = ctx->result;
	
	for(;;)
	{
		//调用epoll_wait()等待I/O就绪事件，并放入result中
		//为了配合时间轮工作，timeout设置为1ms
		int ret = co_epoll_wait(ctx->iEpollFd, result, EPOLL_SIZE, 1);
		
		//active指针指向当前执行环境的pstActiveList队列，注意这里面可能有“活跃的待处理事件”
		stTimeoutItemLink_t *active = (ctx->patActiveList);
		//timeout指针指向pstTimeoutList列表，其实这个列表永远是空的，timeout完全是个临时性的链表
		stTimeoutItemLink_t *timeout = (ctx->pstTimeoutList);
		memset(timeout, 0 ,sizeof(stTimeoutItemLink_t));

		//处理就绪的文件描述符
		for(int i = 0; i < ret;i++)
		{
			//取出就绪事件对应的item
			stTimeoutItem_t *item = (stTimeoutItem_t*)result->events[i].data.ptr;
			//如果定义了预处理函数（在回调前进行预处理），则先进行预处理
			if(item->pfnPrepare)
				//实际上相当于：预处理 + 放入active列表中
				item->pfnPrepare(item, result->events[i], active);
			
			else
				//没有定义预处理函数，直接放入active中
				AddTail(active, item);
		}

		//获取当前事件now
		long long now = GetTickMs();

		//取出所有超时事件，放入timeout链表中（链表中的每一项是一个指针）
		//参数说明：(在ctx中注册的超时事件，现在的时刻，已经超时的事件(返回值))
		TakeAllTimeout( ctx->pTimeout, now, timeout);

		//遍历超时事件链表timeout，将对应事件的已超时标志记为true
		stTimeoutItem_t *lp = timeout->head;
		while(lp)
		{
			lp->Timeout = true;
			lp = lp->next;
		}
		
		//将timeout合并到active中，得到最终的待处理事件链表active
		Join<stTimeoutItem_t,stTimeoutItemLink_t>( active,timeout );

		//遍历active链表，处理事件
		lp = active->head;
		while(lp)
		{
			PopHead(active);
			//调用工作协程设置的pfnProcess()回调函数，resume挂起的工作协程
			//处理对应的IO事件或超时事件
			lp->pfnProcess( lp );

			lp = active->head;
		}
	}
}

