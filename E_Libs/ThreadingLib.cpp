// Win32Lib.cpp
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2011-2013 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#if defined(ES_THREADING)

#include "../EScript/EScript.h"
#include "../EScript/Objects/ReferenceObject.h"
#include "ThreadingLib.h"

#include <iostream>
#include <thread>
#include <mutex>


namespace EScript{


class E_Thread : public Object{
		ES_PROVIDES_TYPE_NAME(Thread)
	public:
		//! (static)
		static Type * getTypeObject(){
			static Type * typeObject = new Type(Object::getTypeObject()); // ---|> Object
			return typeObject;
		}

		ERef<Runtime> rt;
		ObjRef fun;
		std::thread thread;

		E_Thread(ERef<Runtime> _rt,ObjRef _fun,Type* type=nullptr) :
				Object(type?type:getTypeObject()),
				rt(std::move(_rt)), fun(std::move(_fun)){}
		virtual ~E_Thread(){
			std::cout << "~Thread..." << std::endl;
			if(rt)
				rt->_setExitState(nullptr);
			if(thread.joinable())
				thread.join();
			std::cout << "~Thread." << std::endl;
		}

		void run(){
			thread = std::move(std::thread([](E_Thread* eThread){
				eThread->rt->executeFunction( eThread->fun, nullptr, ParameterValues() );
			},this));
		}
		void join(){
			std::cout << "join..." << std::endl;
			thread.join();
			std::cout << "join." << std::endl;
		}

};

class E_Mutex : public ReferenceObject<std::mutex,Policies::SameEObjects_ComparePolicy>{
    ES_PROVIDES_TYPE_NAME(Mutex)
public:
    static Type * getTypeObject(){
        static Type * typeObject = new Type(Object::getTypeObject()); // ---|> Object
        return typeObject;
    }
    E_Mutex() : ReferenceObject(getTypeObject()){}
    virtual ~E_Mutex(){}
};

class E_LockGuard : public ReferenceObject<std::unique_lock<std::mutex>,Policies::SameEObjects_ComparePolicy>{
    ES_PROVIDES_TYPE_NAME(LockGuard)
public:
    static Type * getTypeObject(){
        static Type * typeObject = new Type(Object::getTypeObject()); // ---|> Object
        return typeObject;
    }
    E_LockGuard(std::mutex& inMutex) : ReferenceObject(getTypeObject(),inMutex){}
    virtual ~E_LockGuard(){}
};
}

ES_CONV_EOBJ_TO_OBJ(E_Mutex, std::mutex*, &**eObj)
ES_CONV_EOBJ_TO_OBJ(E_LockGuard, std::unique_lock<std::mutex>*, &**eObj)

namespace EScript{
//! (static)
void ThreadingLib::init(EScript::Namespace * globals) {
	Namespace * lib = new Namespace;
	declareConstant(globals,"Threading",lib);

	{ // thread
		declareConstant(lib, E_Thread::getClassName(), E_Thread::getTypeObject() );

        //! [ESMF]	self Thread.join(  )
        ES_MFUN(E_Thread::getTypeObject(),E_Thread,"join",0,0,(thisObj->join(),thisEObj))

        // isJoinable
        // kill
	}
	{ // mutex
		declareConstant(lib, E_Mutex::getClassName(), E_Mutex::getTypeObject() );

        ES_CTOR(E_Mutex::getTypeObject(), 0, 0, new E_Mutex); // Value::create<E_Mutex>()

        //! [ESMF]	self Mutex.lock(  )
        ES_MFUN(E_Mutex::getTypeObject(), std::mutex, "lock", 0, 0,( thisObj->lock(),thisEObj))

        //! [ESMF]	self Mutex.unlock(  )
        ES_MFUN(E_Mutex::getTypeObject(), std::mutex, "unlock", 0, 0,( thisObj->lock(),thisEObj))

        //! [ESMF]	bool Mutex.tryLock(  )
        ES_MFUN(E_Mutex::getTypeObject(), std::mutex, "tryLock", 0, 0, thisObj->try_lock())
	}

	{ // lockGuard
		declareConstant(lib, E_LockGuard::getClassName(), E_LockGuard::getTypeObject() );
        ES_CTOR(E_LockGuard::getTypeObject(), 1, 1, new E_LockGuard(*parameter[0].to<std::mutex*>(rt)))
	}
	//! [ESF]	void run( fn )
	ES_FUNCTION(lib,"run",1,1,{
		ObjRef eThread(new E_Thread( std::move(rt._fork()),parameter[0] ));
		static_cast<E_Thread*>(eThread.get())->run();
		return std::move(eThread);
	})


 // lock
 // lockGuard
 // tryLock
 // Mutex
 // kill all threads on exit!

}



}
#endif
