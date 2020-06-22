/*
 * poll()内部调用co_poll_inner()
 */
int poll(struct pollfd fds[], nfds_t nfds, int timeout)
{
	return co_poll_inner(co_get_epoll_ct(), fds, nfds,timeout,g_sys_poll_fun);
}

/*
 * @param  ctx		epoll管理器
 * @param  fds[]	fds需要监听的文件描述符
 * @param  nfds		fds的数量
 * @param  timeout	超时毫秒数
 * @param  pollfunc 原始poll函数
 *
 */
int co_poll_inner( stCoEpoll_t *ctx, struct pollfd fds[], nfds_t nfds, int timeout, poll_pfn_t pollfunc)
{
	//1.将poll监听的文件描述符fd交给ctx的epoll进行监视，即由ctx代理监视(poll转化为epoll)
	int epfd = ctx->iEpollFd;
	
	// poll转化为epoll需要用到的结构体
	stPoll_t& arg = *((stPoll_t*)malloc(sizeof(stPoll_t)));
	arg.iEpollFd = epfd;
	arg.fds = (pollfd*)calloc(nfds, sizeof(pollfd));
	arg.nfds = nfds;

	// 设置回调函数，当监视的fd对应事件发生时，将会被调用
	// 该回调函数将会resume()当前协程
	arg.pfnProcess = OnPollProcessEvent;
	
	// 下面将poll需要监听的fd逐一放到epoll中
	for(nfds_t i=0;i<nfds;i++)
	{
		. . .
		// 将fd添加入ctx的epoll中
		co_epoll_ctl(epfd, EPOLL_CTL_ADD, fds[i].fd, &ev );
		. . .
	}


	//2.设置超时事件，放入ctx的pTimeout链表中，如果时间到了但未有监视事件到来，就会触发超时事件
	int ret = AddTimeout( ctx->pTimeout, &arg, now );


	//3.注册完监听事件和超时事件，就yield让出CPU执行权
	co_yield_env( co_get_curr_thread_env() );


	// --------------------分割线---------------------------
	// 注意：！！这个时候，已经和上面的逻辑不在同一个时刻处理了
	
	//4.超时事件超时或者监听的fd事件到来后，主协程将会调用回调函数，也就是resume当前协程

	// 从超时链表中删除对应超时事件和epoll事件
	RemoveFromLink<stTimeoutItem_t,stTimeoutItemLink_t>( &arg );
	for(nfds_t i = 0;i < nfds;i++)
	{
		int fd = fds[i].fd;
		if( fd > -1 )
		{
			//删除epoll事件
			co_epoll_ctl( epfd,EPOLL_CTL_DEL,fd,&arg.pPollItems[i].stEvent );
		}
		//将到来的监视事件放回poll的结果集
		fds[i].revents = arg.fds[i].revents;
	}

	//返回到来的事件数
	int iRaiseCnt = arg.iRaiseCnt;
	return iRaiseCnt;
}
