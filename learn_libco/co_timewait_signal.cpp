/*
 *
 * 语义类似于pthread_cond_signal,用于唤醒一个等待在si条件变量的协程
 * 
 * @param  si: 对应的条件变量
 *
 */
int co_cond_signal( stCoCond_t *si )
{
	//从条件变量si的等待队列中取出一个协程
	stCoCondItem_t * sp = co_cond_pop( si );
	if( !sp )	return 0;
	RemoveFromLink<stTimeoutItem_t,stTimeoutItemLink_t>( &sp->timeout );

	//将取出的协程放到ctx中的activeList上，回调函数会唤醒该协程，
	//主协程负责在eventloop中调用回调函数，resume()唤醒该协程
	AddTail( co_get_curr_thread_env()->pEpoll->pstActiveList,&sp->timeout );

	return 0;
}

/* 
 * 语义上类似于pthread_cond_wait()
 *
 * @param  si: 对应的条件变量
 * @param  ms: 超时毫秒数，<=0表示永不超时
 * 
 */
int co_cond_timewait(stCoCond_t *si, int ms)
{
	//设置了超时时间
	if( ms > 0)
	{
		//在ctx的pTimeout上注册一个超时事件
		. . .
	}

	//将协程放到条件变量si的等待队列上
	AddTail( si, psi);

	//yeild让出协程的CPU执行权
	co_yeild_ct();


	//当前协程被resume()重新获得CPU执行权，有两种情况：超时事件超时或者其他协程
	//调用signal()或broadcast()唤醒当前协程（具体逻辑见上）
	//这时需要将当前协程从条件变量的等待队列中移除
	RemoveFromLink<stCoCondItem_t,stCoCond_t>( psi );
}
