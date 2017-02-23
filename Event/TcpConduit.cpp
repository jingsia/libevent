#include "Config.h"
#include "TcpConduit.h"
#include "TcpServer.h"
#include "Common/TInf.h"
#include "Common/Unzipper.h"

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

namespace Network
{
	TcpConduit::TcpConduit(int fd, TcpSlaveServer * h, int uid): _socket(fd), _host(h), _bev(NULL), _pendclose(false), _uid(uid)
	{
		/*
		 * 可以使用bufferevent_socket_new()创建基于套接字的bufferevent。
		 * base 是event_base，options 是表示bufferevent 选项（BEV_OPT_CLOSE_ON_FREE 等）
		 * 的位掩码，fd 是一个可选的表示套接字的文件描述符。如果想以后设置文件描述符，可以
		 * 设置fd 为-1。
		 * 成功时函数返回一个bufferevent，失败则返回NULL。
		 * */
		_bev = bufferevent_socket_new(_host->getEvBase(), _socket, BEV_OPT_THREADSAFE | BEV_OPT_DEFER_CALLBACKS);
		if(!_bev)
			throw std::bad_alloc();
		/*
		 * bufferevent_setcb()函数修改bufferevent 的一个或者多个回调。readcb、writecb 和eventcb
		 * 函数将分别在已经读取足够的数据、已经写入足够的数据，或者发生错误时被调用。
		 * */
		bufferevent_setcb(_bev, _readcb, _writecb, _eventcb, this);
		bufferevent_enable(_bev, EV_READ);
	}

	TcpConduit::~TcpConduit()
	{
		if(_bev != NULL)
		{
			bufferevent_free(_bev);
			_bev = NULL;
		}
		if(_socket != 0)
		{
			close(_socket);
			_socket = 0;
		}
	}

	void TcpConduit::send(const void * buf, int size)
	{
		if(_bev = NULL)
			return;
		bufferevent_write(_bev, buf, size);
	}

	void TcpConduit::_readcb(struct bufferevent*, void *arg)
	{
		static_cast<TcpConduit *>(arg)->on_read();
	}

	void TcpConduit::_writecb(struct bufferevent*, void * arg)
	{
		static_cast<TcpConduit *>(arg)->on_write();
	}

	void TcpConduit::_eventcb(struct bufferevent *, short wht, void * arg)
	{
		static_cast<TcpConduit *>(arg)->on_event(wht);
	}

	/*
	 * 重要 客户端发送信息 从这里开始
	 * 调用gameclient parsePacket处理数据 读取有用数据
	 * */
	void TcpConduit::on_read()
	{

	}
}
