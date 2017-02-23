/*************************************************************************
	> File Name: TcpServer.h
	> Author: yangjx
	> Mail: yangjx@126.com 
	> Created Time: Sat 18 Feb 2017 06:22:08 AM CST
 ************************************************************************/

#include "Runnable.h"
#include "Network.h"

struct event_base;
struct event;

namespace Network
{

#define WORKERS 2
#define TCP_CONN_IDX_MAX 3

/*
 *	event_base 事件集操作
 * */
class TcpServer:
	public Runnable
{
public:
	TcpServer();
	virtual ~TcpServer();
	virtual void destroy();
	void run();
	virtual void uninit();
	inline struct event_base * getEvBase() { return _ev_base;}

protected:
	void initServer();
	virtual void postInitServer() {}

protected:
	struct event_base * _ev_base;

	bool _running;
};

class TcpMasterServer;

/*
 * event 事件操作
 * */
class TcpSlaveServer:
	public TcpServer
{
	friend class TcpMasterServer;

public:
	TcpSlaveServer(UInt32 idx);

	void initConnection(int = 1);
	void lostConnection(int = 1);
	void accepted(int);
	void remove(int);
	inline UInt32 getCount() { return _count; }
	virtual void destroy();

protected:
	virtual void postInitServer();
	void _accepted(int);
	void _remove(int);

	/*
	 * 纯虚函数
	 * 由子函数实现
	 * */
	virtual TcpConduit * newConduit(int, TcpSlaveServer*, int) = 0;	/* new 一个TcpConduit对象*/
	virtual TcpConduit * newConnection(int, TcpSlaveServer*, int) = 0;

	typedef std::vector<std::shared_ptr<TcpConduit> > _ConduitList;

	static void _ev_op_event(int, short, void *);
	void onOpCheck();

	static void _ev_tick_event(int, short, void *);
	void onTick(UInt32 now);

	template<typename PreType>
	void broadcast(const void *, int, PredType pred);
	void broadcast(const void *, int);

	template<typename ConduitType, Typename PredType, typename DataType>
	void enumerate(PredType pred, DataType);

protected:
	UInt32 _slave_idx;

	_ConduitList _conduits;
	std::set<size_t> _emptySet;

	int _connUp[TCP_CONN_IDX_MAX];
private:
	Mutext _mutex;
	UInt32 _count;

	struct event * _evOp;	/* 事件op  */
	struct event * _evTick; /* 事件tick*/

	FastMutex _opMutex;

	struct _OpStruct
	{
		_OpStruct(UInt8 _t = 0, int _d = 0): type(_t), data(_d) {}
		UInt8 type;
		int data;
	};
	std::vector<_OpStruct> _OpList;
};

class TcpMasterServer:
	public TcpServer
{
public:
	TcpMasterServer(UInt16 port);
	void listen(UInt32 addr, UInt16 port, UInt32 backlog);
	const std::shared_ptr<TcpConduit> find(int id);
	const std::shared_ptr<TcpConduit> findConn(int id);
	void remove(int id);
	void close(int id);
	void closeConn(int id);
	template <typename PredType>
	void broadcast(const void *, int, PredType pred);
	void broadcast(const void *, int);
	template <typename ConduitType, typename PredType>
	void enumerate(PredType pred, DataType);
	UInt32 getCount();
	virtual void uninit();
	virtual void destroy();

protected:
	void postInitServer();
	virtual TcpSlaveServer * newWorker(int) = 0;
	static void _ev_server_event(int, short, void *);
	void on_server_write();
	void on_server_read();
	static void _ev_timer_event(int, short, void *);
	void onTimerCheck();

private:
	int _socket;
	struct event * _ev_server, * _ev_timer;
	std::shared_ptr<TcpConduit> _empty;
	Thread _workerThreads[WORKERS];
	std::vector<std::shared_ptr<TcpSlaveServer> > _workers;
};

/*
 * 主要实现TcpSlaveServer的纯虚函数
 * */
template<class T = TcpConduit>
class TcpSlaveServerT:
	public TcpSlaveServer
{
public:
	TcpSlaveServerT(UInt32 idx): TcpSlaveServer(idx) {}

protected:
	virtual TcpConduit * newConduit(int ss, TcpSlaveServer * s, int id)
	{
		return new(std::nothrow) T(ss, s, id);
	}

	virtual TcpConduit * newConnection(int, TcpSlaveServer *, int)
	{
		return NULL;
	}

};

template<class T = TcpConduit, class SL = TcpSlaveServerT<T>>
class TcpMasterServerT:
	public TcpMasterServer
{
public:
	TcpMasterServerT(UInt16 port): TcpMasterServer(port) {}
protected:
	TcpSlaveServer * newWorker(int idx) { return new SL(idx); }
};

/*
 * 广播 broadcast
 * 将所有的_conduit内容做send操作
 * */
template <typename PredType>
void TcpSlaveServer::broadcast(const void * buf, int len, PredType pred)
{
	Mutex::ScopedLock lk(_mutex);
	for(_ConduitList::iterator it = _conduits.begin(); it != _conduits.end(); ++ it)
	{
		TcpConduit * conduit = (*it).get();
		if(conduit != NULL && conduit->active() && bool(pred(conduit)))
			(*it)->send(buff, len);
	}
}

template<typename ConduitType, typename PredType, typename DataType>
void TcpSlaveServer::enumerate(PredType pred, DataType data)
{
	Mutex::ScopedLock lk(_mutex);
	for(_ConduitList::iterator it = _conduits.begin(); it != _conduits.end(); ++it)
	{
		if((*it).get() != NULL && !pred((ConduitType *)(*it).get(), data))
			return;
	}
}

}

#endif
