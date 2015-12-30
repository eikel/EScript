// RuntimeInternals.h
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2012-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012-2013 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#ifndef ES_RUNTIME_INTERNALS_H
#define ES_RUNTIME_INTERNALS_H

#include "FunctionCallContext.h"
#include "Runtime.h"
#include <unordered_set>

namespace EScript {
class Function;


//! [RuntimeInternals]
class RuntimeInternals  {
		Runtime &runtime;


	//! @name Main
	//	@{
		RuntimeInternals(RuntimeInternals &) = delete;
	public:
		struct SharedRuntimeContext;
		RuntimeInternals(Runtime & rt, ERef<Namespace> globals, std::shared_ptr<SharedRuntimeContext> _sharedRuntimeContext);
		~RuntimeInternals();

		void warn(const std::string& message)const;
	// @}

	// --------------------

	//! @name Multi-Threaded active runtime set
	//	@{
	public:
		struct SharedRuntimeContext{
			std::unordered_set<Runtime*> setOfActiveRuntimeObjects;
			#if defined(ES_THREADING)
			SyncTools::FastLock setOfActiveRuntimeObjectsLock;
			#endif
			// setOfActiveConditionalVariables
		};

		std::shared_ptr<SharedRuntimeContext> getSharedRuntimeContext()const	{	return sharedRuntimeContext;	}
		//! \todo SetOfActiveSignals
		
//		void joinPendingThreads(float timeoutSec);
	private:
		std::shared_ptr<SharedRuntimeContext> sharedRuntimeContext; // for multi-threading; each running thread has an entry

	// @}

	// --------------------

	//! @name Function execution
	//	@{
	public:
		/*! (internal)
			Start the execution of a function. A c++ function is executed immediatly and the result is <result,nullptr>.
			A UserFunction produces a FunctionCallContext which still has to be executed. The result is then result.isFunctionCallContext() == true
			\note the @p params value may be altered by this function and should not be used afterwards!	*/
		RtValue startFunctionExecution(ObjRef fun,ObjRef callingObject,ParameterValues & params);

		RtValue startInstanceCreation(ERef<Type> type,ParameterValues & params);

		ObjRef executeFunctionCallContext(_Ptr<FunctionCallContext> fcc);

		ObjPtr getCallingObject()const							{	return activeFCCs.empty() ? nullptr : activeFCCs.back()->getCaller();	}
		size_t getStackSize()const								{	return activeFCCs.size();	}
		size_t _getStackSizeLimit()const						{	return stackSizeLimit;	}
		void _setStackSizeLimit(const size_t limit)				{	stackSizeLimit = limit;	}


	private:
		_Ptr<FunctionCallContext> getActiveFCC()const			{	return activeFCCs.empty() ? nullptr : activeFCCs.back();	}
		std::vector<_CountedRef<FunctionCallContext> > activeFCCs;
		size_t stackSizeLimit;

		void pushActiveFCC(const _Ptr<FunctionCallContext> & fcc) {
			activeFCCs.push_back(fcc);
			if(activeFCCs.size()>stackSizeLimit) stackSizeError();
		}
		void popActiveFCC()										{	activeFCCs.pop_back();	}
		void stackSizeError();
	// @}

	// --------------------

	//! @name Globals
	//	@{
	public:
		ObjRef getGlobalVariable(const StringId & id);
		Namespace * getGlobals()const;
	private:
		ERef<Namespace> globals;
	// @}

	// --------------------

	//! @name Information
	//	@{
	public:
		int getCurrentLine()const;
		std::string getCurrentFile()const;

		std::string getStackInfo();
		std::string getLocalStackInfo();
	// @}

	// --------------------

	//! @name Internal state / Exceptions
	//	@{
	public:
		//! neither pending exception nor exiting
		bool checkNormalState()const					{	return normalState;	} 
		bool isExceptionPending()const					{	return bool(exceptionValue);	}
		bool isExiting()const							{	return bool(resultValue);	}

		ObjRef fetchAndClearException(){
			#if defined(ES_THREADING)
			SyncTools::FastLockHolder lock(stateLock);
			#endif
			ObjRef result(std::move(exceptionValue));
			normalState = !(result || resultValue);
			return std::move(result);
		}
		ObjRef fetchAndClearExitResult(){
			#if defined(ES_THREADING)
			SyncTools::FastLockHolder lock(stateLock);
			#endif
			ObjRef result(std::move(resultValue));
			normalState = !(result || exceptionValue);
			return std::move(result);
		}

		void setAddStackInfoToExceptions(bool b)			{	addStackInfoToExceptions = b;	}
		
		/*! The given value is set as pending exception. Does NOT throw a C++ exception. */
		void setException(ObjRef value);

		/*! Creates an exception object including current stack info.
			The exception is set as pending exception. Does NOT throw a C++ exception. */
		void setException(std::string s);


		/**
		 * Throws a runtime exception (a C++ Exception, not an internal one!).
		 * Should only be used inside of library-functions
		 * (otherwise, they are not handled and the program is likely to crash).
		 * In all other situations try to use setException(...)
		 */
		void throwException(const std::string & s,Object * obj = nullptr);
		
		void setExitState(ObjRef value);
	
	private:
		#if defined(ES_THREADING)
		SyncTools::atomicBool normalState;
		SyncTools::FastLock stateLock;
		#else
		bool normalState;
		#endif
	
		ObjRef resultValue, exceptionValue;
		bool addStackInfoToExceptions;
	// @}

	// --------------------

	//! @name System calls
	//	@{
	private:
		static bool initSystemFunctions();
	public:
		RtValue sysCall(uint32_t sysFnId,ParameterValues & params);
	//	@}
};
}

#endif // ES_RUNTIME_INTERNALS_H
