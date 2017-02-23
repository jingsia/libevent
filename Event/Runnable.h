/*************************************************************************
	> File Name: Runnable.h
	> Author: yangjx
	> Mail: yangjx@126.com 
	> Created Time: Sat 18 Feb 2017 06:19:06 AM CST
 ************************************************************************/

#ifndef _RUNNABLE_H_
#define _RUNNABLE_H_


class Runnable
{
public:
	Runnable();
	virtual ~Runnable();

	virtual void run() = 0;
};

#endif
