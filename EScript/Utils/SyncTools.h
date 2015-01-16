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
namespace EScript{
namespace SyncTools{

typedef std::lock_guard<std::mutex> MutexHolder;
typedef std::mutex Mutex;


}
}

#endif // SYNCTOOLS_H_INCLUDED
