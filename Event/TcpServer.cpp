/*************************************************************************
	> File Name: TcpServer.cpp
	> Author: yangjx
	> Mail: yangjx@126.com 
	> Created Time: Sat 18 Feb 2017 06:25:53 AM CST
 ************************************************************************/

#include "TcpServer.h"

TcpServer::TcpServer()
	_ev_base(NULL), _running(false)
{
	initServer();
}

TcpServer::~TcpServer()
{
}

/*
 * 创建事件集 event_base_new
 * */
void TcpServer::initServer()
{
	if(_ev_base != NULL)
		return;

	_ev_base = event_base_new(); /* 创建事件集*/

	if(!_ev_base)
	{
		destroy();
		throw std::bad_alloc();
	}
}

void TcpServer::destroy()
{
	if(_ev_base != NULL)
	{
		event_base_free(_ev_base);
		_ev_base = NULL;
	}
}

/*
 * _running 标志 true 做如下操作
 *	init event_base_new 创建新的事件集
 *	event_base_dispatch 分配监听事件集
 *	destroy event_base_free 释放事件集
 * */
void TcpServer::run()
{
	_running = true;
	while(_running)
	{
		{
			initServer();
			postInitServer();
		}
		event_base_dispatch(_ev_base);
		destroy();
	}
}


/*
 * 标志设置 false
 * 目的 停止run操作
 * */
void TcpServer::uninit()
{
	_running = false;
}


////////////////////////////////////////////////////////////////

TcpSlaveServer::TcpSlaveServer(UInt32 idx): TcpServer(), _slave_idx(idx), _count(0), _evOp(NULL), _evTick(NULL)    
{
	for(int i = 0; i < TCP_CONN_IDX_MAX; ++i)
		_connUp[i] = -1; 
}


/*
 * 分发调用 event_base_dispatch
 * */
/*
 * 创建事件
 *
 * struct event event_new(struct event_base ,evutil_socket_t ,short ,event_callback_fn,void*)
 *
 * 参数一：事件所在的事件集。
 * 参数二：socket的描述符。
 * 参数三：事件类型，其中EV_READ表示等待读事件发生，EV_WRITE表示写事件发生，或者它俩的组合，EV_SIGNAL表示需要等待事件的号码，如 果不包含上述的标志，就是超时事件或者手动激活的事件。
 * 参数四：事件发生时需要调用的回调函数。
 * 参数五：回调函数的参数值。
 * */
void TcpSlaveServer::postInitServer()
{
	_evOp = event_new(_ev_base, -1, EV_PERSIST, _ev_op_event, this);	/* EV_PERSIST 表示事件一直存在*/
	if(_evOp != NULL)
	{
		struct timeval tv = {0, 250000};
		event_add(_evOp, &tv);
	}

	_evTick = event_new(_ev_base, -1, EV_PERSIST, _ev_tick_event, this);
	if(_evTick != NULL)
	{
		struct timeval tv = {1, 0};
		event_add(_evTick, &tv);
	}
}

/*
 * socket 接收消息
 * ss 区别玩家
 * */
void TcpSlaveServer::accepted(int ss)
{
	FastMutex::ScopedLock lk(_opMutex);
	_opList.push_back(_OpStruct(1, ss));
}

/*
 * 事件_evOp 的 回调函数 arg = this 参数五
 * */
void TcpSlaveServer::_ev_op_event(int, short, void * arg)
{
	static_cast<TcpSlaveServer *>(arg)->onOpCheck();
}

/*
 * _ev_op_event 调用
 * */
void TcpSlaveServer::onOpCheck()
{
	if(!_running)
	{
		/*
		 * 这是 event_base_dispatch的更灵活版本。默认情况下，这个循环会一直运行，直到没有添加的事件，或者直到调用了event_base_loopbreak()或者evenet_base_loopexit().你可以通过flags参数修改这个行为。
		 * */
		event_base_loopbreak(_ev_base);
	}

	_opMutex.lock();	/* 加锁 */
	if(_opList.empty())
		_opMutex.unlock(); /* 解锁 */
	else
	{
		std::vector<OpStruct> rlist = _opList;	/* 处理 _opList */
		_opList.clear();
		_opMutex.unlock();	/* 解锁 */
		size_t sz = rlist.size();
		for(size_t i = 0; i < sz; ++i)
		{
			_opStruct& _os = rlist[i];
			if(_os.type == 1)
				_accepted(_os.data);	/* 处理函数 accept */
			else
				_remove(_os.data);		/* 处理函数 remove */
		}
	}
}

void TcpSlaveServer::_accepted(int ss)
{
	TcpConduit * conduit = NULL;
	_mutex.lock();

	while(!_emptySet.empty())
	{
		std::set<size_t>::iterator it = _emptySet.begin();
		size_t id = *it;
		if(id < _conduits.size())
		{
			try
			{
				if(ss < 0)
				{
					try
					{
						conduit = newConnection(ss, this, id * WORKERS + _slave_idx);
						if(conduit)
							conduit->initConnection();
					} catch(...)
					{
						if(conduit)
							delete conduit;
						conduit = NULL;
					}
				}
				else
				{
					
				}
			}
		}
	}
}


TcpMasterServer::TcpMasterServer(UInt16 port):
	TcpServer(), _socket(0), _ev_server(NULL)
{
	listen(INADDR_ANY, port, 8);
	/*
	 * 用于对套接字进行非阻塞IO 的调用也不能移植到Windows 中。
	 * evutil_make_socket_nonblocking()函数要求一个套接字（来自socket()或者accept()）作为
	 * 参数，将其设置为非阻塞的。（设置Unix 中的O_NONBLOCK 标志和Windows 中的FIONBIO
	 * 标志）
	 * */
	evutil_make_socket_noblocking(_socket);
	make_linger(_socket);
}

/*
 * 网络操作
 * */
void TcpMasterServer::listen(UInt32 addr, UInt16 port, UInt32 backlog)
{
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(_socket < 0)
		throw std::bad_exception();
	struct sockaddr_in addr2 = {0};
	addr2.sin_family = AF_INET;
	addr2.sin_addr.s_addr = htonl(addr);
	int r = 1;
	setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&r, sizeof(r));
	if(bind(_socket, (const sockaddr *)&addr2, sizeof(addr2)) < 0)
		throw std::bad_exception();
	if(::listen(_socket, backlog) < 0)
		throw std::bad_exception();
}

void TcpMasterServer::on_server_write()
{
	/*
	 * event_base_loopbreak（）让event_base立即退出循环。
	 * 它与event_base_loopexit（base,NULL）的不同在于，如果event_base当前正在执行激活事件的回调，它将在执行完当前正在处理的事件后立即退出。
	 * */
	event_base_loopbreak(_ev_base);
}

void TcpMasterServer::postInitServer()
{
	for(UInt32 i = 0; i < WORKERS; ++i)
	{
		TcpSlaveServer * s = newWorker(i);
		_workers.push_back(std::shared_ptr<TcpSlaveServer>(s));
	}
	for(int i = 0; i < WORKERS; ++i)
	{
		_workerThread[i].start(*_workers[i]);
	}

	/*
	 * 创建事件
	 *
	 * struct event event_new(struct event_base ,evutil_socket_t ,short ,event_callback_fn,void*)
	 *
	 * 参数一：事件所在的事件集。
	 * 参数二：socket的描述符。
	 * 参数三：事件类型，其中EV_READ表示等待读事件发生，EV_WRITE表示写事件发生，或者它俩的组合，EV_SIGNAL表示需要等待事件的号码，如 果不包含上述的标志，就是超时事件或者手动激活的事件。
	 * 参数四：事件发生时需要调用的回调函数。
	 * 参数五：回调函数的参数值。
	 * */
	_ev_server = event_new(_ev_base, _socket, EV_READ | EV_WRITE | EV_PERSIST, _ev_server_event, this);

	if(_ev_server == NULL)
	{
		close(_socket);
		destroy();
		throw std::bad_alloc();
	}

	/*
	 * int event_add(struct event * ev,const struct timeval* timeout)
	 *
	 * 参数一：需要添加的事件
	 * 参数二：事件的最大等待事件，如果是NULL的话，就是永久等待
	 * */
	event_add(_ev_server, NULL);

	_ev_timer = event_new(_ev_base, -1, EV_PERSIST, _ev_timer_event, this);
	if(_ev_timer != NULL)
	{
		struct timeval tv = {5, 0};
		event_add(_ev_timer, &tv);
	}
}

void TcpMasterServer::on_server_read()
{
	while(1)
	{
		struct sockaddr_in addr = {0};
		socklen_t l = sizeof(addr);
		/*
		 * sock 标志
		 * */
		int sock = accept(_socket, (struct sockaddr *)&addr, &l);
		if(sock < 0)
			return;

		/*
		 * 用于对套接字进行非阻塞IO 的调用也不能移植到Windows 中。
		 * evutil_make_socket_nonblocking()函数要求一个套接字（来自socket()或者accept()）作为
		 * 参数，将其设置为非阻塞的。（设置Unix 中的O_NONBLOCK 标志和Windows 中的FIONBIO
		 * 标志）
		 * */
		evutil_make_socket_nonblocking(sock);
		make_linger(sock);
		UInt32 min_cap = _workers[0]->getCount();
		int idx = 0;
		for(int i = 1; i < WORKERS; ++i)
		{
			UInt32 cap = _workers[i]->getCount();
			if(cap < min_cap)
			{
				min_cap = cap;
				idx = i;
			}
			_workers[idx]->accepted(sock);
		}
	}
}

void TcpMasterServer::_ev_server_event(int fd, short fl, void *arg)
{
	TcpMasterServer * svr = static_cast<TcpMasterServer *>(arg);
	if(fl & EV_WRITE)
		svr->on_server_write();
	else
		svr->on_server_read();
}






