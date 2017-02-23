#ifndef _TCPSERVERWRAPPER_H_
#define _TCPSERVERWRAPPER_H_

#include <memory>

#include "GameClient.h"
#include "TcpServer.h"
#include "Common/Stream.h"

typedef std::shared_ptr<Network::TcpConduit> TcpConnection; /* tcp 连接*/

namespace Network
{
	class TcpSlaveWrapper:
		public TcpSlaveServerT<GameClient>
	{
	public:
		TcpSlaveWrapper(UInt32 idx): TcpSlaveServerT<GameClient>(idx) {}
		virtual TcpConduit* newConnection(int ss, TcpSlaveServer *s, int id) { return NULL;}
	};

	class TcpServerWrapper
	{
	public:
		/*
		 * 程序启动时候会调用
		 * port 程序启动端口
		 * */
		TcpServerWrapper(UInt16 port)
		{
			m_TcpService = new TcpMasterServerT<GameClient, TcpSlaveWrapper>(port);
			assert(m_TcpService != NULL);
			m_Active = true;
		}

		~TcpServerWrapper()
		{
			delete m_TcpService;
		}

	public:
		inline void Start()
		{
			m_TcpThread.start(*m_TcpService);
		}
		inline void Join()
		{
			m_TcpThread.join();
		}
		inline void UnInit()
		{
			m_Active = false;
			m_TcpService->uninit();
		}

		inline void Close(int sessionID)
		{
			if(!m_Active)
				return;
			m_TcpService->close(sessionID);
		}
		inline void CloseArena()
		{
			if(!m_Active)
				return;
			m_TcpService->closeConn(-1);
		}
		inline void CloseServerWar()
		{
			if(!m_Active)
				return;
			m_TcpService->closeConn(-2);
		}
		inline void CloseServerLeft()
		{
			if(!m_Active)
				return;
			m_TcpService->closeConn(-3);
		}

		inline TcpConnection GetConn(int sessionID)
		{
			if(!m_Active)
				return TcpConnection();

			return m_TcpService->find(sessionID);
		}

	public:
		template <typename MsgType>
		void SendMsgToClient(int sessionID, MsgType& msg);
	};

	private:
		bool m_Active;
		Thread m_TcpThread;
		/*
		 * 这个 重要 
		 * 将 gameclient tcpserver tcpSlaveServer 联系在一起
		 * */
		TcpMasterServerT<Network::GameClient, TcpSlaveWrapper>* m_TcpService;
}

#endif
