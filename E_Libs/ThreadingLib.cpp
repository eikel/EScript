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
		virtual ~E_Thread(){}

		void run(){
			thread = std::move(std::thread([](E_Thread* eThread){
				eThread->rt->executeFunction( eThread->fun, nullptr, ParameterValues() );
			},this));
		}
};


//! (static)
void ThreadingLib::init(EScript::Namespace * globals) {
	Namespace * lib = new Namespace;
	declareConstant(globals,"Threading",lib);
	
	{ // thread
		declareConstant(lib, E_Thread::getClassName(), E_Thread::getTypeObject() );
		
	}
	

//	struct Starter{
//		ERef<Runtime rt> _t;
//		ObjRef fun;
//		Starter(ERef<Runtime rt> _rt,ObjRef _fun) : rt(std::move(_rt)),fun(std::move(_fun)){}
//		void operator(){
//			
//		}
//	};
	
	//! [ESF]	void run( fn )
	ES_FUNCTION(lib,"run",1,1,{
		E_Thread* eThread = new E_Thread( std::move(rt._fork()),parameter[0] );
		eThread->run();
		return eThread;
	})
//	
//	//!	[ESF] ExtObject new ExtObject( [Map objAttributes] )
//	ES_CONSTRUCTOR(typeObject,0,1,{
//		ERef<ExtObject> result(new ExtObject(thisType));
//		if(parameter.count()>0){
//			Map * m = assertType<Map>(rt,parameter[0]);
//			for(const auto & keyValuePair : *m) {
//				result->setAttribute(keyValuePair.first, Attribute(keyValuePair.second.value));
//			}
//		}
//		return result.detachAndDecrease();
//	})

	
//
//	//! [ESF]	void setClipboard( string )
//	ES_FUN(lib,"setClipboard",1,1,(Win32Lib::setClipboard(parameter[0].toString()),RtValue(nullptr)))
//
//	//! [ESF]	string getClipboard( )
//	ES_FUN(lib,"getClipboard",0,0,Win32Lib::getClipboard())
//
//	//! [ESF]	bool loadLibrary(string )
//	ES_FUNCTION(lib,"loadLibrary",1,1, {
//		HINSTANCE hDLL;
//		libInitFunction *  f;	// Function pointer
//
//		hDLL = LoadLibrary(parameter[0].toString().c_str());
//		if(hDLL == nullptr)
//			return false;
//
//		f = reinterpret_cast<libInitFunction*>(GetProcAddress(hDLL,"init"));
//		if(!f)	{
//			// handle the error
//			FreeLibrary(hDLL);
//			return false;
//		}
//		EScript::initLibrary(f);
//
//		return true;
//	})

}



}
#endif
