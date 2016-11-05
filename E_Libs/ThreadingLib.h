// ThreadingLib.h
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2015 Claudius Jähn <ClaudiusJ@live.de>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#ifndef THREADINGLIB_H
#define THREADINGLIB_H

#if defined(ES_THREADING)
#include <string>

namespace EScript{
class Namespace;

namespace ThreadingLib {

//LIB_EXPORT
void init(EScript::Namespace * o);

}
}
#endif // ES_THREADING

#endif // THREADINGLIB_H
