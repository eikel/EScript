// SyncTools.h
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2015 Claudius Jähn <ClaudiusJ@live.de>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#ifndef SYNCTOOLS_H_INCLUDED
#define SYNCTOOLS_H_INCLUDED

#include <thread>
#include <mutex>
#include <atomic>

namespace EScript{
namespace SyncTools{
namespace _Internals{
//! \see http://en.cppreference.com/w/cpp/atomic/atomic_flag
class SpinLock{
	private:
		std::atomic_flag f;
	public:
		SpinLock():f(ATOMIC_FLAG_INIT){}

		void lock()		{	while(!f.test_and_set(std::memory_order_acquire));	}
		bool try_lock()	{	return !f.test_and_set(std::memory_order_acquire);	}
		void unlock()	{	f.clear(std::memory_order_release);		}
};
}

typedef std::atomic<int> atomicInt;
typedef std::atomic<bool> atomicBool;
//typedef std::mutex  FastLock;
typedef _Internals::SpinLock FastLock;
typedef std::unique_lock<FastLock> FastLockHolder;

}
}

#endif // SYNCTOOLS_H_INCLUDED
