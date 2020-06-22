//生产者生产的“资源”，消费者消费的“资源”
struct stTask_t
{
	int id;
};
//生产者/消费者协程共享的临界区（协程通信）
struct stEnv_t
{
	stCoCond_t* cond;
	queue<stTask_t*> task_queue;
};

/* 
 * 生产者协程
 * @param  args：生产者/消费者通过此参数获取临界区
 *
 */
void* Producer(void* args)
{
	co_enable_hook_sys();
	//获取参数
	stEnv_t* env=  (stEnv_t*)args;
	int id = 0;
	while (true)
	{
		//生产一个资源，放入共享的临界区中
		stTask_t* task = (stTask_t*)calloc(1, sizeof(stTask_t));
		task->id = id++;
		env->task_queue.push(task);
		printf("%s:%d produce task %d\n", __func__, __LINE__, task->id);

		//唤醒等待队列中其中一个协程（broadcast才是唤醒所有）
		co_cond_signal(env->cond);
		//调用hook过的poll()，在ctx上注册一个超时事件（回调函数是“唤醒”当前协程），然后yeild让出CPU执行权
		poll(NULL, 0, 1000);
		//poll()注册的超时事件超时的时候，主协程调用回调函数，resume“唤醒”当前协程
	}
	return NULL;
}

/*
 * @param args: 生产者/消费者通过此参数获取临界区
 *
 */
void* Consumer(void* args)
{
	co_enable_hook_sys();
	stEnv_t* env = (stEnv_t*)args;
	while (true)
	{
		//没有资源，调用timewait()进入等待队列，yeild让出CPU执行权
		if (env->task_queue.empty())
		{
			co_cond_timedwait(env->cond, -1);
			//当生产者调用signal()时，当前（消费者）协程被resume唤醒
			continue;
		}

		//消费一个资源
		stTask_t* task = env->task_queue.front();
		env->task_queue.pop();
		printf("%s:%d consume task %d\n", __func__, __LINE__, task->id);
		free(task);
	}
	return NULL;
}
int main()
{
	stEnv_t* env = new stEnv_t;
	env->cond = co_cond_alloc();

	//创建消费者协程，并resume
	stCoRoutine_t* consumer_routine;
	co_create(&consumer_routine, NULL, Consumer, env);
	co_resume(consumer_routine);

	//创建生产者协程，并resume
	stCoRoutine_t* producer_routine;
	co_create(&producer_routine, NULL, Producer, env);
	co_resume(producer_routine);
	
	//主协程eventloop开始调度
	co_eventloop(co_get_epoll_ct(), NULL, NULL);
	return 0;
}
