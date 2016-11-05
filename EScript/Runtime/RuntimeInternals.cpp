// RuntimeInternals.cpp
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2012-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#include "RuntimeInternals.h"
#include "FunctionCallContext.h"
#include "../EScript.h"
#include "../Utils/StringUtils.h"
#include "../Objects/Callables/FnBinder.h"
#include "../Objects/Callables/Function.h"
#include "../Objects/Exception.h"
#include "../Objects/YieldIterator.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace EScript{
	
// -------------------------------------------------------------
// System calls

typedef RtValue ( * sysFunctionPtr)(RuntimeInternals &,const ParameterValues &);
static sysFunctionPtr systemFunctions[Consts::NUM_SYS_CALLS];

bool RuntimeInternals::initSystemFunctions(){
	#define ES_SYS_FUNCTION(_name) \
	static EScript::RtValue _name(	EScript::RuntimeInternals & rtIt UNUSED_ATTRIBUTE, \
									const EScript::ParameterValues & parameter UNUSED_ATTRIBUTE)

	#define ESSF(_fnName, _min, _max, _returnExpr) \
	ES_SYS_FUNCTION(_fnName) { \
		EScript::assertParamCount(rtIt.runtime, parameter.count(), _min, _max); \
		return (_returnExpr); \
	}

	//! init system calls \note the order of the functions MUST correspond to their funcitonId as defined in Consts.h
	{	//! [ESSF] Array SYS_CALL_CREATE_ARRAY( param* )
		struct _{
			ESSF( sysCall,0,-1,Array::create(parameter) )
		};
		systemFunctions[Consts::SYS_CALL_CREATE_ARRAY] = _::sysCall;
	}
	{	//! [ESSF] Map SYS_CALL_CREATE_MAP( key0,value0, key1,value1, ... )
		struct _{
			ES_SYS_FUNCTION( sysCall) {
					if( (parameter.count()%2)==1 ) rtIt.warn("Map: Last parameter ignored!");
					Map * a = Map::create();
					for(ParameterValues::size_type i = 0;i<parameter.count();i+=2)
						a->setValue(parameter[i],parameter[i+1]);
				return a;
			}
		};
		systemFunctions[Consts::SYS_CALL_CREATE_MAP] = _::sysCall;
	}
	{	//! [ESSF] Void SYS_CALL_THROW_TYPE_EXCEPTION( expectedType, receivedValue )
		struct _{
			ES_SYS_FUNCTION( sysCall) {
				assertParamCount(rtIt.runtime,parameter,2,-1);
				std::ostringstream os;
				os << "Parameter check failed! \nValue: "<<parameter[parameter.size()-1]->toDbgString()<<"\nConstraints: ";
				for(size_t i = 0;i<parameter.size()-1;++i ){
					if(i>0) os <<" || ";
					ObjRef obj = parameter[i];
					os<<(obj ? obj->toDbgString() : "???");
				}
				rtIt.setException(os.str());
				return nullptr;
			}
		};
		systemFunctions[Consts::SYS_CALL_THROW_TYPE_EXCEPTION] = _::sysCall;
	}
	{	//! [ESSF] Void SYS_CALL_THROW( [value] )
		struct _{
			ESSF( sysCall,0,1,(rtIt.runtime.setException( parameter.count()>0 ? parameter[0] : nullptr ),RtValue(nullptr)))
		};
		systemFunctions[Consts::SYS_CALL_THROW] = _::sysCall;
	}
	{	//! [ESSF] Void SYS_CALL_EXIT( [value] )
		struct _{
			ESSF( sysCall,0,1,(rtIt.runtime._setExitState( parameter.count()>0 ? parameter[0] : nullptr ),RtValue(nullptr)))
		};
		systemFunctions[Consts::SYS_CALL_EXIT] = _::sysCall;
	}
	{	//! [ESSF] Iterator SYS_CALL_GET_ITERATOR( object );
		struct _{
			ES_SYS_FUNCTION( sysCall) {
				assertParamCount(rtIt.runtime,parameter.count(),1,1);
				ObjRef it;
				if(	Collection * c = parameter[0].castTo<Collection>()){
					it = c->getIterator();
				}else if(parameter[0].castTo<YieldIterator>()){
					it = parameter[0].get();
				}else {
					it = std::move(callMemberFunction(rtIt.runtime,parameter[0] ,Consts::IDENTIFIER_fn_getIterator,ParameterValues()));
				}
				if(it==nullptr){
					rtIt.setException("Could not get iterator from '" + parameter[0]->toDbgString() + '\'');
					return nullptr;
				}
				return it.detachAndDecrease();
			}
		};
		systemFunctions[Consts::SYS_CALL_GET_ITERATOR] = _::sysCall;
	}
	{	//! [ESSF] Void SYS_CALL_TEST_ARRAY_PARAMETER_CONSTRAINTS( expectedTypes*, Array receivedValue )
		struct _{
			ES_SYS_FUNCTION( sysCall) {
				assertParamCount(rtIt.runtime,parameter.count(),2,-1);
				const size_t constraintEnd = parameter.size()-1;

				Array * values = assertType<Array>(rtIt.runtime,parameter[constraintEnd]);

				for(const auto & val : *values) {
					bool success = false;
					for(size_t i = 0; i<constraintEnd; ++i){
						ObjRef result = std::move(callMemberFunction(rtIt.runtime,parameter[i] ,Consts::IDENTIFIER_fn_checkConstraint,ParameterValues(val.get())));
						if(result.toBool()) {
							success = true;
							break;
						}
					}
					if(!success){
						std::ostringstream os;
						os << "Parameter check failed! \nValue: "<<val->toDbgString()<<"\nConstraints: ";
						for(size_t i = 0;i<constraintEnd;++i ){
							if(i>0) os <<" || ";
							ObjRef obj = parameter[i];
							os<<(obj ? obj->toDbgString() : "???");
						}
						rtIt.setException(os.str());
						return nullptr;
					}
				}
				return nullptr;
			}
		};
		systemFunctions[Consts::SYS_CALL_TEST_ARRAY_PARAMETER_CONSTRAINTS] = _::sysCall;
	}
	{	//! [ESSF] Void SYS_CALL_EXPAND_PARAMS_ON_STACK( numberOfParams, steps* )
		struct _{
			ES_SYS_FUNCTION( sysCall) {
				// parameters[i>0] contain number of stack entries that have to be stored to get to the next expanding parameter
				auto & rt = rtIt.runtime;
				auto fcc = rtIt.getActiveFCC();
				uint32_t numParams = parameter[0].to<uint32_t>(rt); // original number of parameters
				std::vector<RtValue> tmpStackStorage;
				// for each expanding parameter..
				for(int i = static_cast<int>(parameter.count())-1;i>0;--i){
					// pop and store non expanding parameters
					for(uint32_t j = parameter[i].to<uint32_t>(rt); j>0; --j)
						tmpStackStorage.emplace_back(fcc->stack_popValue());

					// pop expanding array parameter
					ObjRef expandingParam( std::move(fcc->stack_popObject()));
					Array * arr = assertType<Array>(rt,expandingParam);
					numParams += arr->size();
					--numParams;	// the extracted array is no parameter

					// store array values
					for(auto it=arr->rbegin();it!=arr->rend();++it)
						tmpStackStorage.push_back(*it);
				}

				// push stored values
				while(!tmpStackStorage.empty()){
					fcc->stack_pushValue(std::move(tmpStackStorage.back()));
					tmpStackStorage.pop_back();
				}
				// push new parameter count by returning it
				return numParams;
			}
		};
		systemFunctions[Consts::SYS_CALL_EXPAND_PARAMS_ON_STACK] = _::sysCall;
	}
	{	/*! [ESSF] bool SYS_CALL_CASE_TEST( object );
			If the parameter equals the topmost stack content, the stack is popped and true is returned,
				false is returned otherwise.
		*/
		struct _{
			ES_SYS_FUNCTION( sysCall) {
				assertParamCount(rtIt.runtime,parameter.count(),1,1);
				auto fcc = rtIt.getActiveFCC();
				ObjRef decisionValue = fcc->stack_popObject();
				if( parameter[0]->isEqual(rtIt.runtime,decisionValue.get()) ){
					// decisionValue is consumed
					return true;
				}else{
					fcc->stack_pushObject(decisionValue); // push back the decisionValue
					return false;
				}
			}
		};
		systemFunctions[Consts::SYS_CALL_CASE_TEST] = _::sysCall;
	}
	{	// @(once)
		/*! [ESSF] bool SYS_CALL_ONCE_ENTER( ) : pop onceMarkerId;
			[ESSF] bool SYS_CALL_ONCE_LEAVE( ) : pop onceMarkerId;
			if thisFn has an attribute named @p onceMarker id, true (=skip) is returned.
			else a corresponding attribute is initialized and false (=do not skip) is returned.
		*/
		struct _{
			ES_SYS_FUNCTION( sysCall_enter ) {
				auto fcc = rtIt.getActiveFCC();
				const StringId markerId = fcc->stack_popIdentifier();
				
				auto theActiveFunction =  fcc->getUserFunction();
				Attribute* markerAttr;
				{
					#if defined(ES_THREADING)
					SyncTools::FastLockHolder attrLock( theActiveFunction->attributesMutex );
					#endif
					
					markerAttr = theActiveFunction->objAttributes.accessAttribute(markerId);
					if( !markerAttr ){
						theActiveFunction->objAttributes.setAttribute(markerId,Bool::create(false));
						return false; // execute the once block
					}
				}
				while( !markerAttr->getValue()->toBool() ){ 
					//! wait on marker
					//std::cout << ".";
				}
				return true; // already called -> skip statement
			}
			ES_SYS_FUNCTION( sysCall_leave ) {
				auto fcc = rtIt.getActiveFCC();
				const StringId markerId = fcc->stack_popIdentifier();
				auto theActiveFunction =  fcc->getUserFunction();
				{
					#if defined(ES_THREADING)
					SyncTools::FastLockHolder attrLock( theActiveFunction->attributesMutex );
					#endif
					auto markerAttr = theActiveFunction->objAttributes.accessAttribute(markerId);
					Bool* markerValue = markerAttr ? markerAttr->getValue().toType<Bool>() : nullptr;
					if(!markerValue)
						throw std::runtime_error("RuntimeInternals: SYS_CALL_ONCE_LEAVE marker not found!");
					markerValue->setValue(true);
				}
				return nullptr;
			}
		};
		systemFunctions[Consts::SYS_CALL_ONCE_ENTER] = _::sysCall_enter;
		systemFunctions[Consts::SYS_CALL_ONCE_LEAVE] = _::sysCall_leave;

	}
	{	/*! [ESSF] vale SYS_CALL_GET_STATIC_VAR( ) : pop uint32 staticVarLocation;
		*/
		struct _{
			ES_SYS_FUNCTION( sysCall ) {
				auto fcc = rtIt.getActiveFCC();
				const uint32_t staticVarIdx = fcc->stack_popUInt32();
				return fcc->getStaticVar(staticVarIdx);
			}
		};
		systemFunctions[Consts::SYS_CALL_GET_STATIC_VAR] = _::sysCall;
	}
	{	/*! [ESSF] vale SYS_CALL_SET_STATIC_VAR( ) : pop uint32 staticVarLocation, pop value;
		*/
		struct _{
			ES_SYS_FUNCTION( sysCall ) {
				auto fcc = rtIt.getActiveFCC();
				const uint32_t staticVarIdx = fcc->stack_popUInt32();
				ObjRef value = fcc->stack_popObject();
				if(value)
					value = std::move(value->getRefOrCopy());
				 fcc->setStaticVar(staticVarIdx,value.get());
				return nullptr;
			}
		};
		systemFunctions[Consts::SYS_CALL_SET_STATIC_VAR] = _::sysCall;
	}
	return true;
}

// ---------------------------------------------------------------

//! (ctor)
RuntimeInternals::RuntimeInternals(Runtime & rt,ERef<Namespace> _globals,std::shared_ptr<SharedRuntimeContext> _sharedRuntimeContext) :
		runtime(rt), sharedRuntimeContext(std::move(_sharedRuntimeContext)),
		stackSizeLimit(100000),globals(std::move(_globals)),normalState(true),addStackInfoToExceptions(true){
	static bool once( initSystemFunctions() );
	(void)once;
	
	{
		#if defined(ES_THREADING)
		SyncTools::FastLockHolder lock(sharedRuntimeContext->setOfActiveRuntimeObjectsLock);
		#endif
		sharedRuntimeContext->setOfActiveRuntimeObjects.insert(&rt);
		std::cout << "####+" << sharedRuntimeContext->setOfActiveRuntimeObjects.size();
	}
}

RuntimeInternals::~RuntimeInternals(){
	{
		#if defined(ES_THREADING)
		SyncTools::FastLockHolder lock(sharedRuntimeContext->setOfActiveRuntimeObjectsLock);
		#endif
		sharedRuntimeContext->setOfActiveRuntimeObjects.erase(&runtime);
		std::cout << "####~" << sharedRuntimeContext->setOfActiveRuntimeObjects.size();
	}
}

//void RuntimeInternals::joinPendingThreads(float timeoutSec){
//	(void)timeoutSec;
//
//
//}

		
// -------------------------------------------------------------
// Function execution

RtValue RuntimeInternals::sysCall(uint32_t sysFnId,ParameterValues & params){
	if(sysFnId>=Consts::NUM_SYS_CALLS){
		std::ostringstream os;
		os << "(internal) Unknown systemCall #"<<sysFnId<<'.';
		runtime.setException(os.str());
		return nullptr;
	}
	return (systemFunctions[sysFnId])(*this,params);
}

//! (internal)
ObjRef RuntimeInternals::executeFunctionCallContext(_Ptr<FunctionCallContext> fcc){

	fcc->enableStopExecutionAfterEnding();
	pushActiveFCC(fcc);

//	std::cout << fcc->getInstructions().toString()<<"\n";

	while( true ){
		if(!normalState){
			#if defined(ES_THREADING)
			SyncTools::FastLockHolder lock(stateLock);
			#endif
			if(!normalState){ // re-check after locking
				if(isExceptionPending()){
					while(true){
						fcc->stack_clear(); // remove current stack content

						// catch-block available?
						if(fcc->getExceptionHandlerPos()!=Instruction::INVALID_JUMP_ADDRESS){
							// don't call fetchAndClearException() as we already keep stateLock
							ObjRef except(std::move(exceptionValue));
							fcc->assignToLocalVariable(Consts::LOCAL_VAR_INDEX_internalResult,std::move(except)); // ___result = exceptionResult
							fcc->setInstructionCursor(fcc->getExceptionHandlerPos());
							normalState = !(exceptionValue || resultValue);
							break;
						} // execution stops here? Keep the exception-state and return;
						else if(fcc->isExecutionStoppedAfterEnding()){
							popActiveFCC();
							return nullptr;
						} // continue with the next fcc...
						else{
							popActiveFCC();
							fcc = getActiveFCC();
							if(fcc.isNull())
								return nullptr;
						}
					}
				} else if(isExiting()){
					while(true){
						fcc->stack_clear();
						// execution stops here? Keep the exiting-state and return;
						if(fcc->isExecutionStoppedAfterEnding()){
							popActiveFCC();
							return nullptr;
						} // continue with the next fcc...
						else{
							popActiveFCC();
							fcc = getActiveFCC();
							if(fcc.isNull())
								return nullptr;
						}
					}
				}else {
					throw std::runtime_error("RuntimeInternals: Invalid internal state.");
				}
			}
			
		}
		
		const std::vector<Instruction> & instructions = fcc->getInstructions();

		// end of function? continue with calling function
		if(fcc->getInstructionCursor() == instructions.end()){
			ObjRef result = fcc->getLocalVariable(Consts::LOCAL_VAR_INDEX_internalResult);
			if(fcc->isConstructorCall()){
				if( result ){
					warn("Constructors should not return a value.");
				}
				// \note the local variable $0 contains the created object, "fcc->getCaller()" contains the instanciated Type-Object.
				result = fcc->getLocalVariable(Consts::LOCAL_VAR_INDEX_this);
			}
			if(fcc->stack_size()!=0){
				std::cout <<fcc->stack_size() <<" ";
				setException("(internal) FCC-Stack contains invalid value.");
				break;
			}
			if(fcc->isExecutionStoppedAfterEnding()){
				popActiveFCC();
				return result;
			}
			popActiveFCC();

			const bool useResultAsCaller = fcc->isProvidingCallerAsResult();// providesCaller
			fcc = getActiveFCC();
			if(fcc.isNull()){ //! just to be safe (should never occur)
				return result;
			}

			if(useResultAsCaller){
				fcc->initCaller(result);
			}else{
				if(result)
					result = std::move(result->getRefOrCopy());
				fcc->stack_pushValue(std::move(RtValue(std::move(result))));
			}
			continue;
		}


		// --------------------------------------------------------------------------------------------------------------
		// Instructio execution...
		try{

		const Instruction & instruction = *fcc->getInstructionCursor();

//		std::cout << "---\n";
//		std::cout << fcc->getCaller().toString()<<"\n";
//		std::cout << fcc->stack_toDbgString()<<"\n";
//		std::cout << instruction.toString(fcc->getInstructionBlock())<<"\n";

		/* \note
			Use a 'break' to end a case to check the state before continuing.
			In other words: Use 'continue' only if no exception or warning may occur.*/
		switch(instruction.getType()){

		case Instruction::I_ASSIGN_ATTRIBUTE:{
			/*	object = popObject
				value = popValueObject
				if object.identifier and not const and not private then object.identifier = value	*/
			ObjRef obj( std::move(fcc->stack_popObject()) );
			ObjRef value( std::move(fcc->stack_popObjectValue()) );

			Object::AttributeReference_t attrHolder( std::move(obj->_accessAttribute(instruction.getValue_Identifier(),false) ));
			Attribute* const attr = std::get<0>(attrHolder);
			if(attr){
				if(attr->getProperties()&Attribute::ASSIGNMENT_RELEVANT_BITS){
					if(attr->isConst()){
#if defined(ES_THREADING)
						std::get<1>(attrHolder).unlock(); // [2014-01-18] Strange deadlock workaround.
#endif
						setException("Cannot assign to const attribute '"+instruction.getValue_Identifier().toString()+"'.");
						break;
					}else if(attr->isPrivate() && fcc->getCaller()!=obj ) {
#if defined(ES_THREADING)
						std::get<1>(attrHolder).unlock(); // [2014-01-18] Strange deadlock workaround.
#endif
						setException("Cannot access private attribute '"+instruction.getValue_Identifier().toString()+"' from outside of its owning object.");
						break;
					}
				}
				attr->setValue(value.get());
			}else{
				warn("Attribute not found: '"+instruction.getValue_Identifier().toString()+'\'');
			}
			fcc->increaseInstructionCursor();
			break;
		}
		case Instruction::I_ASSIGN_LOCAL:{
			/* 	assignLocal (uint32_t) variableIndex
				------------
				pop value
				$variableIndex = value	*/
			fcc->assignToLocalVariable(instruction.getValue_uint32(), std::move(fcc->stack_popObjectValue()));
			fcc->increaseInstructionCursor();
			continue;
		}

		case Instruction::I_ASSIGN_VARIABLE:{
			/*	value = popValueObject
				if caller.identifier then caller.identifier = value
				else if globals.identifier then globals.identigier = value
				else warning */
			ObjRef value( std::move(fcc->stack_popObjectValue()) );

			Object::AttributeReference_t attrHolder;
			Attribute * attr = nullptr;
			
			if( fcc->getCaller() ){
				attrHolder =  std::move(fcc->getCaller()->_accessAttribute(instruction.getValue_Identifier(),false));
				attr = std::get<0>(attrHolder);
			}
			if(!attr){
				attrHolder =  std::move(globals->_accessAttribute(instruction.getValue_Identifier(),true));
				attr = std::get<0>(attrHolder);
			}
			if(attr){
				if(attr->isConst()){
#if defined(ES_THREADING)
					std::get<1>(attrHolder).unlock(); // [2014-01-18] Strange deadlock workaround.
#endif
					setException("Cannot assign to const attribute '"+instruction.getValue_Identifier().toString()+"'.");
				}else{
					attr->setValue(value.get());
				}
			}else{
				warn("Attribute not found: '"+instruction.getValue_Identifier().toString()+'\'');
			}
			fcc->increaseInstructionCursor();
			break;
		}
		case Instruction::I_CALL:{
			/*	call (uint32_t) numParams
				-------------
				pop numParams * parameters
				pop function
				pop object
				call the function
				push result (or jump to exception point)	*/
			uint32_t numParams = instruction.getValue_uint32();
			if(numParams==Consts::DYNAMIC_PARAMETER_COUNT) // the parameter count is dynamic and lies on the stack.
				numParams = fcc->stack_popUInt32();

			//! \todo check once if the stack is big enough

			ParameterValues params(numParams);
			for(int i = static_cast<int>(numParams)-1;i>=0;--i )
				params.emplace(i,fcc->stack_popObjectValue());

			ObjRef fun( std::move(fcc->stack_popObject()) );
			ObjRef caller( std::move(fcc->stack_popObject()) );

			// returnValue , newUserFunctionCallContext
			RtValue result( std::move(startFunctionExecution(fun,caller,params)) );
			fcc->increaseInstructionCursor();
			if(result.isFunctionCallContext()){ // user function?
				fcc = result._getFCC();
				pushActiveFCC(fcc);
			}else{
				fcc->stack_pushValue(std::move(result));
			}

			break;
		}
		case Instruction::I_CREATE_INSTANCE:{
			/*	create (uint32_t) numParams
				-------------
				pop numParams many parameters
				pop object
				call object._constructor
				push result (or jump to exception point)	*/


			uint32_t numParams = instruction.getValue_uint32();
			if(numParams==Consts::DYNAMIC_PARAMETER_COUNT) // the parameter count is dynamic and lies on the stack.
				numParams = fcc->stack_popUInt32();
			ParameterValues params(numParams);
			// pop the parameters for the first constructor
			for(int i = static_cast<int>(numParams)-1;i>=0;--i )
				params.emplace(i,fcc->stack_popObjectValue());

			// pop objects whose constructor is called
			ObjRef caller( std::move(fcc->stack_popObject()) );
			Type* typePtr = caller.castTo<Type>();
			if(!typePtr){
				setException("Can't instantiate object not of type 'Type'");
				break;
			}
			// manual move
			caller.detach();
			ERef<Type> typeRef;
			typeRef._set(typePtr);

			// start instance creation
			RtValue result(std::move(startInstanceCreation(std::move(typeRef),params)));
			fcc->increaseInstructionCursor();
			if(result.isFunctionCallContext()){ // user constructor?
				fcc = result._getFCC();
				pushActiveFCC(fcc);
			}else{ // direct call to c++ constructor
				fcc->stack_pushValue(std::move(result));
			}
			break;
		}
		case Instruction::I_DUP:{
			// duplicate topmost stack entry
			fcc->stack_dup();
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_FIND_VARIABLE:{
			/*	if caller.Identifier -> push (caller, caller.Identifier)
				else push (GLOBALS, GLOBALS.Identifier) (or nullptr,nullptr + Warning) 	*/
			if(fcc->getCaller()){
				Attribute attr(std::move(fcc->getCaller()->getAttribute(instruction.getValue_Identifier())));
				if(attr){
					fcc->stack_pushObject(fcc->getCaller());
					fcc->stack_pushObject(attr.extractValue());
					fcc->increaseInstructionCursor();
					continue;
				}
			}
			ObjRef obj(std::move(getGlobalVariable(instruction.getValue_Identifier())));
			if(obj){
				fcc->stack_pushObject(globals.get());
				fcc->stack_pushObject(std::move(obj));
			}else{
				warn("Variable '"+instruction.getValue_Identifier().toString()+"' not found: ");
				fcc->stack_pushVoid();
				fcc->stack_pushVoid();
			}
			fcc->increaseInstructionCursor();
			break;
		}
		case Instruction::I_GET_ATTRIBUTE:{
			/*	pop Object
				push Object.Identifier (or nullptr + Warning)	*/
			ObjRef obj( std::move(fcc->stack_popObject()) );
			Attribute attr( std::move(obj->getAttribute(instruction.getValue_Identifier())) );
			if(!attr) {
				warn("Attribute not found: '"+instruction.getValue_Identifier().toString()+'\'');
				fcc->stack_pushVoid();
			}else if(attr.isPrivate() && fcc->getCaller()!=obj ) {
				setException("Cannot access private attribute '"+instruction.getValue_Identifier().toString()+"' from outside of its owning object.");
				break;
			}else{
				fcc->stack_pushObject( attr.getValue() );
			}
			fcc->increaseInstructionCursor();
			break;
		}
		case Instruction::I_GET_VARIABLE:{
			/*	if caller.Identifier -> push (caller.Identifier)
				else push (GLOBALS.Identifier) (or nullptr + Warning) 	*/
			if(fcc->getCaller()){
				Attribute attr( std::move(fcc->getCaller()->getAttribute(instruction.getValue_Identifier())) );
				if(attr){
					fcc->stack_pushObject( std::move(attr.extractValue()) );
					fcc->increaseInstructionCursor();
					continue;
				}
			}
			ObjRef obj(std::move(getGlobalVariable(instruction.getValue_Identifier())));
			if(obj){
				fcc->stack_pushObject(std::move(obj));
			}else{
				warn("Variable not found: '"+instruction.getValue_Identifier().toString()+'\'');
				fcc->stack_pushVoid();
			}
			fcc->increaseInstructionCursor();
			break;
		}
		case Instruction::I_GET_LOCAL_VARIABLE:{
			/* 	getLocalVariable (uint32_t) variableIndex
				------------
				push $variableIndex	*/
			fcc->stack_pushObject( fcc->getLocalVariable(instruction.getValue_uint32())) ;
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_INIT_CALLER:{
			const uint32_t numParams = instruction.getValue_uint32();

			if(fcc->isConstructorCall()){

				// pop super constructor parameters
				ParameterValues params(numParams);
				for(int i = static_cast<int>(numParams)-1;i>=0;--i )
					params.emplace(i,fcc->stack_popObjectValue());

				// pop next super constructor
				ObjRef superConstructor( std::move(fcc->stack_popObjectValue()) );

				// pop remaining super constructors
				std::vector<ObjPtr> constructors;
				while(!fcc->stack_empty())
					constructors.emplace_back( std::move(fcc->stack_popObject()) );

				// call next super constructor
				RtValue result( std::move(startFunctionExecution(superConstructor,fcc->getCaller(),params)) );
				fcc->increaseInstructionCursor();

				if(result.isFunctionCallContext()){
					// pass remaining super constructors to new calling context
					fcc = result._getFCC();
					pushActiveFCC(fcc);
					for(std::vector<ObjPtr>::const_reverse_iterator it = constructors.rbegin();it!=constructors.rend();++it)
						fcc->stack_pushObject( *it );
					fcc->markAsConstructorCall(); // the result of the called super constructor should be used as this-object.
					fcc->markAsProvidingCallerAsResult(); // providesCallerAsResult

				}else{
					ObjPtr newObj = result.getObject();
					if(newObj.isNull()){
						if(!isExceptionPending()) // if an exception occurred in the constructor, the result may be NULL
							setException("Constructor did not create an Object."); //! \todo improve message!
						break;
					}
					// init attributes
					newObj->_initAttributes(runtime);
					fcc->initCaller(newObj);
				}

				break;
			}else{ // no constructor call
				fcc->increaseInstructionCursor();
				if(numParams>0){
					warn("Calling constructor function with @(super) attribute as normal function.");
					break;
				}
				continue;
			}
		}
		case Instruction::I_JMP:{
			fcc->setInstructionCursor( instruction.getValue_uint32() );
			continue;
		}
		case Instruction::I_JMP_IF_SET:{
			/* 	jmpIfSet (uint32) targetAddress
				-------------
				pop (uint32) local variable index
				jmp if variable != nullptr */
			if( fcc->getLocalVariable( fcc->stack_popUInt32() )!=nullptr )
				fcc->setInstructionCursor( instruction.getValue_uint32() );
			else
				fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_JMP_ON_TRUE:{
			if(fcc->stack_popBool())
				fcc->setInstructionCursor( instruction.getValue_uint32() );
			else
				fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_JMP_ON_FALSE:{
			if(!fcc->stack_popBool())
				fcc->setInstructionCursor( instruction.getValue_uint32() );
			else
				fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_NOT:{
			/*	not
				-------
				bool b = popBool
				push !b	*/
			fcc->stack_pushBool( !fcc->stack_popBool() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_POP:{
			// remove entry from stack
			fcc->stack_pop();
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_BOOL:{
			fcc->stack_pushBool( instruction.getValue_Bool() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_ID:{
			fcc->stack_pushIdentifier( instruction.getValue_Identifier() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_FUNCTION:{
			fcc->stack_pushFunction( instruction.getValue_uint32() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_NUMBER:{
			fcc->stack_pushNumber( instruction.getValue_Number() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_STRING:{
			fcc->stack_pushStringIndex( instruction.getValue_uint32() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_UINT:{
			fcc->stack_pushUInt32( instruction.getValue_uint32() );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_UNDEFINED:{
			fcc->stack_pushUndefined();
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_PUSH_VOID:{
			fcc->stack_pushVoid( );
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_RESET_LOCAL_VARIABLE:{
			// $localVarId = nullptr
//			fcc->assignToLocalVariable(instruction.getValue_uint32(), nullptr);
			fcc->resetLocalVariable(instruction.getValue_uint32());
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_SET_ATTRIBUTE:{
			/*	setAttribute identifierId
				-------------
				properies = pop uint32
				obj = pop object
				value = pop parameterObj
				obj.identifier @(properties) := value	*/

			const uint32_t properties = fcc->stack_popUInt32();
			ObjRef obj( std::move(fcc->stack_popObject()) );
			ObjRef value( std::move(fcc->stack_popObjectValue()) );

			if( (properties & Attribute::OVERRIDE_BIT)){
				Object::AttributeReference_t attrHolder( std::move(obj->_accessAttribute(instruction.getValue_Identifier(),false) ));
				if(!std::get<0>(attrHolder))
					warn("Attribute marked with @(override) does not override.");
			}
			if( (properties & Attribute::TYPE_ATTR_BIT) && obj->_getInternalTypeId() != _TypeIds::TYPE_TYPE) {
				warn("Setting type attribute '"+instruction.getValue_Identifier().toString()+"' to an object which is no Type.");
			}
			if(!obj->setAttribute(instruction.getValue_Identifier(),Attribute(value,properties))){
				warn("Could not set attribute '"+instruction.getValue_Identifier().toString()+"'.");
			}
			fcc->increaseInstructionCursor();
			break;
		}
		case Instruction::I_SET_EXCEPTION_HANDLER:{
			fcc->setExceptionHandlerPos(instruction.getValue_uint32());
			fcc->increaseInstructionCursor();
			continue;
		}
		case Instruction::I_SYS_CALL:{
			/*	sysCall (uint32_t,uint32_t) numParams, instruction
				-------------
				pop numParams * parameters
				sysCall functionId,parameters
				push result (or jump to exception point)	*/
			const std::pair<uint32_t,uint32_t> v = instruction.getValue_uint32Pair();

			const uint32_t funId = v.first;
			const uint32_t numParams = (v.second == Consts::DYNAMIC_PARAMETER_COUNT) ?
											fcc->stack_popUInt32() :
											v.second;

			ParameterValues params(numParams);
			for(int i = static_cast<int>(numParams)-1;i>=0;--i )
				params.emplace(i,fcc->stack_popObjectValue());

			RtValue result(std::move(sysCall(funId,params)));
			fcc->increaseInstructionCursor();
			if(result.isFunctionCallContext()){ // user function?
				fcc = result._getFCC();
				pushActiveFCC(fcc);
			}else{
				fcc->stack_pushValue( std::move(result) );
			}
			break;
		}
		case Instruction::I_YIELD:{
			/*	yield
				-------------
				pop result	*/
			ObjRef value( std::move(fcc->stack_popObjectValue()) );
			ERef<YieldIterator> yIt = new YieldIterator;
			yIt->setFCC(fcc);
			yIt->setValue(value.get());
			fcc->increaseInstructionCursor();
			if(fcc->isExecutionStoppedAfterEnding()){
				popActiveFCC();
				return yIt.detachAndDecrease();
			} // continue with the next fcc...
			else{
				popActiveFCC();
				fcc = getActiveFCC();
				if(fcc.isNull())
					return nullptr;
				fcc->stack_pushObject(yIt.get());
			}
			break;
		}
		case Instruction::I_UNDEFINED:
		case Instruction::I_SET_MARKER:
		default:{
			fcc->increaseInstructionCursor();
			warn("Unknown Instruction");
		}
		}
		}catch(Object * e){
			setException(dynamic_cast<Exception*>(e));
		}
		// --------------------------------------------------------------------------------------------------------------
		
	}
	// -----------
	return Void::get();
}

//! (internal)
RtValue RuntimeInternals::startFunctionExecution(ObjRef fun, ObjRef _callingObject,ParameterValues & pValues){
	if(fun.isNull()){
		setException("No function to call!");
		return RtValue();
	}
	switch( fun->_getInternalTypeId() ){
		case _TypeIds::TYPE_USER_FUNCTION:{
			// manual move
			UserFunction * userFunction = static_cast<UserFunction*>(fun.detach()); 
			ERef<UserFunction> userFunRef;
			userFunRef._set(userFunction);
			_CountedRef<FunctionCallContext> fcc = FunctionCallContext::create(std::move(userFunRef), std::move(_callingObject));
			// fcc holds the reference to fun and _callingObject, userFunction can safely be used.

			// check for too few parameter values -> throw exception
			if(userFunction->getMinParamCount()>=0 && pValues.size()<static_cast<size_t>(userFunction->getMinParamCount())){
				std::ostringstream os;
				os << "Too few parameters: Expected " << userFunction->getMinParamCount() << ", got " << pValues.size() << '.';
				pushActiveFCC(fcc); // temporarily activate the fcc to add the last level to the stackInfo.
				setException(os.str());
				popActiveFCC();
				return RtValue();
			}
			uint32_t variableIdx = Consts::LOCAL_VAR_INDEX_firstParameter;
			const int maxParamCount = userFunction->getMaxParamCount();
			if(maxParamCount<0){ // multiParameter
				const int multiParamIndex = userFunction->getMultiParam();
				ParameterValues::const_iterator valueIt = pValues.begin();

				// assign parameter values coming before the multi parameter
				if(multiParamIndex>0){
					const auto firstMultiParamValue = std::min( pValues.end(), std::next(pValues.begin(),userFunction->getMultiParam()) );
					while(valueIt<firstMultiParamValue){
						fcc->assignToLocalVariable(variableIdx,*valueIt);
						++variableIdx;
						++valueIt;
					}
				}
				if(fcc->getLocalVariableName(variableIdx).empty()){ // empty parameter name? -> ignore the values
					++variableIdx;
					valueIt = std::next(pValues.begin(), pValues.size()+1+multiParamIndex-userFunction->getParamCount());
					for(; valueIt<pValues.end(); ++valueIt,++variableIdx) // assign the remaining values
						fcc->assignToLocalVariable(variableIdx,*valueIt);
				}else if(valueIt>=pValues.end()){ // multi parameter lies behind the actually given parameters: fn(a=1,m...){} ()
					fcc->assignToLocalVariable(Consts::LOCAL_VAR_INDEX_firstParameter+multiParamIndex, Array::create()); // assign an empty array
				}else { // copy values into multiParam
					EPtr<Array> multiParamArray = Array::create();
					ObjRef arrayRef(multiParamArray.get());
					const auto pValuesEnd = std::next(pValues.begin(), pValues.size()+1+multiParamIndex-userFunction->getParamCount());
					while( valueIt<pValuesEnd ){
						multiParamArray->pushBack( *valueIt );
						++valueIt;
					}
					fcc->assignToLocalVariable(variableIdx++,arrayRef);
					for(; valueIt<pValues.end(); ++valueIt,++variableIdx) // assign the remaining values
						fcc->assignToLocalVariable(variableIdx,*valueIt);
				}
			} // too many parameters
			else if( pValues.size()>static_cast<size_t>(maxParamCount) ){
				std::ostringstream os;
				os<<"Too many parameters given: Expected "<<maxParamCount<<", got "<<pValues.size()<<'.';
				warn(os.str());
				const auto pValuesEnd = std::next(pValues.begin(), maxParamCount);
				for(ParameterValues::const_iterator it = pValues.begin(); it!= pValuesEnd; ++it,++variableIdx)
					fcc->assignToLocalVariable(variableIdx,*it);
			}else{ // normal parameter count range
				for(ParameterValues::const_iterator it = pValues.begin(); it!= pValues.end(); ++it,++variableIdx)
					fcc->assignToLocalVariable(variableIdx,*it);
			}
			return RtValue::createFunctionCallContext(fcc.detachAndDecrease());
		}
		case _TypeIds::TYPE_FN_BINDER:{
			FnBinder * binder = static_cast<FnBinder*>(fun.get());
			if(binder->getBoundParameters().empty()){
				return startFunctionExecution(binder->getFunction(),
												binder->getObject()?binder->getObject():_callingObject,
												pValues);
			}else{
				ParameterValues pValues2(pValues.size()+binder->getBoundParameters().size());
				size_t i=0;
				for(auto& param : binder->getBoundParameters())
					pValues2.set(i++, param);
				for(auto& param : pValues)
					pValues2.set(i++, param);
				return startFunctionExecution(binder->getFunction(),
												binder->getObject()?binder->getObject():_callingObject,
												pValues2);
			}
		}
		case _TypeIds::TYPE_FUNCTION:{ // is  C++ function ?
			Function * libfun = static_cast<Function*>(fun.get());
			{	// check param count
				const int min = libfun->getMinParamCount();
				const int max = libfun->getMaxParamCount();
				if( (min>0 && static_cast<int>(pValues.count())<min)){
					std::ostringstream os;
					os<<"Too few parameters: Expected " <<min<<", got "<<pValues.count()<<'.';
					setException(os.str());
					return RtValue();
				} else  if(max>=0 && static_cast<int>(pValues.count())>max) {
					std::ostringstream os;
					os<<"Too many parameters: Expected " <<max<<", got "<<pValues.count()<<'.';
					warn(os.str());
				}
			}
			libfun->increaseCallCounter();

			try {
				return (*libfun->getFnPtr())(runtime,_callingObject.get(),pValues);
			} catch(const char * message) {
				setException(std::string("C++ exception: ")+message);
			} catch(const std::string & message) {
				setException(std::string("C++ exception: ") + message);
			} catch(const std::exception & e) {
				setException(std::string("C++ exception: ") + e.what());
			} catch(Object * obj) {
				Exception * e = dynamic_cast<Exception *>(obj);
				if(e){
					if(addStackInfoToExceptions)
						e->setStackInfo(getStackInfo());
					setException(e);
				}else{
					setException(obj);
				}
			} catch(...){
				setException("C++ exception");
			}
			return RtValue();
		}

		default:{
			// function-object has a user defined "_call"-member?
			Attribute attr( std::move(fun->getAttribute(Consts::IDENTIFIER_fn_call)) );		//! \todo check for @(private)

			if(attr){
				// fun._call( callingObj , param0 , param1 , ... )
				ParameterValues pValues2(pValues.count()+1);
				pValues2.set(0,_callingObject ? _callingObject : nullptr);
				std::copy(pValues.begin(),pValues.end(),pValues2.begin()+1);

				return startFunctionExecution(attr.getValue(),fun,pValues2);
			}
			warn("Cannot use '"+fun->toDbgString()+"' as a function.");
		}
	}
	return RtValue();
}


//! (internal)
RtValue RuntimeInternals::startInstanceCreation(ERef<Type> type,ParameterValues & pValues){ // add caller as parameter?
	std::vector<ObjPtr> constructors;

	// collect constructors
	for(Type* typeCursor = type.get(); typeCursor; typeCursor = typeCursor->getBaseType()){
		Object::AttributeReference_t attrHolder(typeCursor->_accessAttribute(Consts::IDENTIFIER_fn_constructor,true));
		const Attribute * ctorAttr = std::get<0>(attrHolder);
		if(ctorAttr){
			// first constructor must not be private -- unless it is an attribute of the calling object or of a base class (needed for factory functions!)
			if(constructors.empty() && ctorAttr->isPrivate() && !typeCursor->isBaseOf( getCallingObject().castTo<Type>() )){
#if defined(ES_THREADING)
				std::get<1>(attrHolder).unlock(); // [2014-01-18] Strange deadlock workaround.
#endif
				setException("Can't instantiate Type with private _contructor.");
				return RtValue(); // failure
			}
			ObjPtr fun = ctorAttr->getValue();
			constructors.push_back(fun);
			if(fun->_getInternalTypeId()==_TypeIds::TYPE_FUNCTION) // factory function found
				break;
		}
	}

	// call the outermost constructor and pass the other constructor-functions by adding them to the stack
	if(!constructors.empty()) {
		ObjRef typeRef; // manual move
		typeRef._set(type.detach());
		RtValue result( std::move(startFunctionExecution(constructors.front(),std::move(typeRef),pValues)) );
		if(result.isFunctionCallContext()){
			FunctionCallContext * fcc = result._getFCC();
			for(std::vector<ObjPtr>::const_reverse_iterator it = constructors.rbegin(); std::next(it) != constructors.rend(); ++it)
				fcc->stack_pushObject(*it);
			fcc->markAsConstructorCall();
			return RtValue::createFunctionCallContext(fcc);
		}else if(result.isObject()){
			// init attributes
			result._getObject()->_initAttributes(runtime);
			return result;
		}
	}
	if(!isExceptionPending()) // if no exception occurred in the constructor, the result may be nullptr
		setException("Constructor failed to create an object.");
	return RtValue(); // failure
}

// -------------------------------------------------------------
// Globals
Namespace * RuntimeInternals::getGlobals()const	{
	return globals.get();
}

ObjRef RuntimeInternals::getGlobalVariable(const StringId & id) {
	// \note getLocalAttribute is used to skip the members of Type
	// 	which are otherwise found as false global variables  [BUG20100618]
	return std::move(globals->getLocalAttribute(id).extractValue());
}


// -------------------------------------------------------------
// Information

std::string RuntimeInternals::getCurrentFile()const{
	if(getActiveFCC()){
		return getActiveFCC()->getUserFunction()->getCode().getFilename();
	}
	return std::string();
}

int RuntimeInternals::getCurrentLine()const{
	if(getActiveFCC()){
		return getActiveFCC()->getCurrentLine();
	}else{
		return -1;
	}
}

std::string RuntimeInternals::getLocalStackInfo(){
	_Ptr<FunctionCallContext> fcc = getActiveFCC();
	return fcc ? fcc->stack_toDbgString() : "";
}

std::string RuntimeInternals::getStackInfo(){
	std::ostringstream os;
	os<<"\n\n----------------------\nCall stack:";
	int nr = 0;
	const int skipStart = activeFCCs.size()>50 ? 20 : activeFCCs.size()+1;
	const int skipEnd = activeFCCs.size()>50 ? activeFCCs.size()-20 : 0;
	for(std::vector<_CountedRef<FunctionCallContext> >::reverse_iterator it = activeFCCs.rbegin();it!=activeFCCs.rend();++it){
		++nr;
		if( nr>=skipStart && nr<skipEnd){
			continue;
		}else if(nr==skipStart){
			os<<"\n\n ... \n";
		}else{
			const _CountedRef<FunctionCallContext> & fcc = *it;
			const EPtr<UserFunction> activeFun = fcc->getUserFunction();
			const int activeLine = fcc->getCurrentLine();
			os<<"\n\n"<<nr<<'.'<<
				"\t("<< activeFun->getCode().getFilename()<<":"<<activeLine<<')';

			if(activeLine>=0){
				os<< "\nCode:\t'"<< StringUtils::trim(StringUtils::getLine(activeFun->getCode().getFullCode(),activeLine-1)) <<"'";
			}
			os<<"\nFun:\t" << (fcc->getCaller() ? fcc->getCaller()->toDbgString() : "undefined")<<
				" -> "<<fcc->getUserFunction()->toDbgString();
			if(nr==1){
				os<<"\nLocals:\t" << fcc->getLocalVariablesAsString(false);
			}
			if(fcc->getExceptionHandlerPos()!=Instruction::INVALID_JUMP_ADDRESS){
				os<<"\n\\_____Catches_exceptions_____/";
			}
			// \note this does not work properly: If the last call failed because of too few parameter values, the marking may not be correct.
			if(fcc->isExecutionStoppedAfterEnding()){
				os<<"\n\n---"; // c++-call
			}
		}
	}
	os<<"\n\n----------------------\n";
	return os.str();
}
// -------------------------------------------------------------
// State / Exceptions

void RuntimeInternals::setException(std::string s) {
	ERef<Exception> e( new Exception(std::move(s),getCurrentLine()) );
	e->setFilename(getCurrentFile());
	if(addStackInfoToExceptions)
		e->setStackInfo(getStackInfo());
	setException(e.get());
}

void RuntimeInternals::setException(ObjRef value) {
	#if defined(ES_THREADING)
	SyncTools::FastLockHolder lock(stateLock);
	#endif
	exceptionValue = std::move(value ? value : Void::get());
	normalState = false;
}

void RuntimeInternals::setExitState(ObjRef value) {
	#if defined(ES_THREADING)
	SyncTools::FastLockHolder lock(stateLock);
	#endif
	resultValue = std::move(value ? value : Void::get());
	normalState = false;
}

void RuntimeInternals::throwException(const std::string & s,Object * obj) {
	std::ostringstream os;
	os<<s;
	if(obj) os<<'('<<obj->toString()<<')';
	if(addStackInfoToExceptions)
		os<<getStackInfo();
	Exception * e = new Exception(os.str(),getCurrentLine());
	e->setFilename(getCurrentFile());
	throw e;
}

void RuntimeInternals::stackSizeError(){
	std::ostringstream os;
	os << "The number of active functions ("<<getStackSize()<< ") reached its limit.";
	setException(os.str());
}

void RuntimeInternals::warn(const std::string & s)const {
	std::ostringstream os;
	os << s;
	if(getActiveFCC()){
		os<<" ('" << getActiveFCC()->getUserFunction()->getCode().getFilename() << "':~"<<getCurrentLine()<<")";
	}
	runtime.getLogger()->warn(os.str());
}

// -------------------------------------------------------------

}
