// Compiler.h
// This file is part of the EScript programming language.
// See copyright notice in EScript.h
// ------------------------------------------------------
#include "Compiler.h"
#include "CompilerContext.h"
#include "../Objects/typeIds.h"
#include "../Objects/AST/BlockStatement.h"
#include "../Objects/AST/ConditionalExpr.h"
#include "../Objects/AST/ForeachStatement.h"
#include "../Objects/AST/FunctionCallExpr.h"
#include "../Objects/AST/GetAttributeExpr.h"
#include "../Objects/AST/IfStatement.h"
#include "../Objects/AST/LogicOpExpr.h"
#include "../Objects/AST/LoopStatement.h"
#include "../Objects/AST/SetAttributeExpr.h"
#include "../Objects/AST/Statement.h"
#include "../Objects/AST/TryCatchStatement.h"
#include "../Objects/Callables/UserFunction.h"
#include "../Objects/Values/Bool.h"
#include "../Objects/Values/Number.h"
#include "../Objects/Values/String.h"
#include "../Objects/Values/Void.h"
#include <stdexcept>
#include <map>

#if !defined(_MSC_VER) and !defined(UNUSED_ATTRIBUTE)
#define UNUSED_ATTRIBUTE __attribute__ ((unused))
#else
#define UNUSED_ATTRIBUTE
#endif

namespace EScript{

// init handlerRegistry \todo Can't this be done more efficiently using c++11 functionals???
struct handler_t{ virtual void operator()(CompilerContext & ctxt,ObjPtr obj)=0; };
typedef std::map<internalTypeId_t,handler_t *> handlerRegistry_t;
static bool initHandler(handlerRegistry_t &);
static handlerRegistry_t handlerRegistry;
static bool _handlerInitialized UNUSED_ATTRIBUTE = initHandler(handlerRegistry);

void Compiler::compileExpression(CompilerContext & ctxt,ObjPtr expression)const{
	if(expression.isNull())
		return;
	const internalTypeId_t typeId = expression->_getInternalTypeId();
	
	handlerRegistry_t::iterator it = handlerRegistry.find(typeId);
	if(it==handlerRegistry.end()){
			std::cout << reinterpret_cast<void*>(typeId)<<"\n";
		throw std::invalid_argument("Expression can't be compiled.");
	}
	(*it->second)(ctxt,expression);
}


// ------------------------------------------------------------------


//! (static)
bool initHandler(handlerRegistry_t & m){
	// \note  the redundant assignment to 'id2' is a workaround to a strange linker error ("undefined reference EScript::_TypeIds::TYPE_NUMBER")
	#define ADD_HANDLER( _id, _type, _block) \
	{ \
		struct _handler : public handler_t{ \
			~_handler(){} \
			virtual void operator()(CompilerContext & ctxt,ObjPtr obj){ \
				_type * self = obj.toType<_type>(); \
				if(!self) throw std::invalid_argument("Wrong type!"); \
				do _block while(false); \
			} \
		}; \
		const internalTypeId_t id2 = _id; \
		m[id2] = new _handler(); \
	}
	// ------------------------
	// Simple types

	// Number
	ADD_HANDLER( _TypeIds::TYPE_NUMBER, Number, {
		ctxt.addInstruction(Instruction::createPushNumber(self->toDouble()));
	})
	// Bool
	ADD_HANDLER( _TypeIds::TYPE_BOOL, Bool, {
		ctxt.addInstruction(Instruction::createPushNumber(self->toBool()));
	})	
	// String
	ADD_HANDLER( _TypeIds::TYPE_STRING, String, {
		ctxt.addInstruction(Instruction::createPushString(ctxt.declareString(self->toString())));
	})	
	// Void
	ADD_HANDLER( _TypeIds::TYPE_VOID, Void, {
		ctxt.addInstruction(Instruction::createPushVoid());
	})


	// ------------------------
	// AST 
	using namespace AST;

	
	// BlockStatement
	ADD_HANDLER( _TypeIds::TYPE_BLOCK_STATEMENT, BlockStatement, {
		if(self->hasLocalVars()) 
			ctxt.pushSetting_localVars(*self->getVars());

		for ( BlockStatement::statementCursor c = self->getStatements().begin();  c != self->getStatements().end(); ++c) {
			c->_asm(ctxt);
		}
		if(self->hasLocalVars()){
			for(std::set<StringId>::const_iterator it = self->getVars()->begin();it!=self->getVars()->end();++it){
				ctxt.addInstruction(Instruction::createResetLocalVariable(ctxt.getCurrentVarIndex(*it)));
			}
			ctxt.popSetting();
		}
	})
	
	// ConditionalExpr
	ADD_HANDLER( _TypeIds::TYPE_CONDITIONAL_EXPRESSION, ConditionalExpr, {
		if(self->getCondition().isNull()){
			if(self->getElseAction().isNotNull()){
				ctxt.compile(self->getElseAction());
			}
		}else{
			const uint32_t elseMarker = ctxt.createMarker();
		
			ctxt.compile(self->getCondition());
			ctxt.addInstruction(Instruction::createJmpOnFalse(elseMarker));
			
			ctxt.compile( self->getAction() );
			
			if(self->getElseAction().isNotNull()){
				const uint32_t endMarker = ctxt.createMarker();
				ctxt.addInstruction(Instruction::createJmp(endMarker));
				ctxt.addInstruction(Instruction::createSetMarker(elseMarker));
				ctxt.compile( self->getElseAction() );
				ctxt.addInstruction(Instruction::createSetMarker(endMarker));
			}else{
				ctxt.addInstruction(Instruction::createSetMarker(elseMarker));
			}
		}
	})
	
	// ForeachStatement
	ADD_HANDLER( _TypeIds::TYPE_FOREACH_STATEMENT, ForeachStatement, {
		// \todo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	})
	
	
	// FunctionCallExpr
	ADD_HANDLER( _TypeIds::TYPE_FUNCTION_CALL_EXPRESSION, FunctionCallExpr, {
		do{
			GetAttributeExpr * gAttr = self->getGetFunctionExpression().toType<GetAttributeExpr>();

			// getAttributeExpression (...)
			if( gAttr ){
				const StringId attrId = gAttr->getAttrId();

				if(gAttr->getObjectExpression()==NULL){ // singleIdentifier (...)
					const int localVarIndex = ctxt.getCurrentVarIndex(attrId);
					if(localVarIndex>=0){
						ctxt.addInstruction(Instruction::createPushVoid());
						ctxt.addInstruction(Instruction::createGetLocalVariable(localVarIndex));
					}else{
						ctxt.addInstruction(Instruction::createFindVariable(attrId));
					}
					break;
				} // getAttributeExpression.identifier (...)
				else if(GetAttributeExpr * gAttrGAttr = gAttr->getObjectExpression().toType<GetAttributeExpr>() ){
					ctxt.compile(gAttrGAttr);
					ctxt.addInstruction(Instruction::createDup());
					ctxt.addInstruction(Instruction::createGetAttribute(attrId));
					break;
				} // somethingElse.identifier (...) e.g. foo().bla(), 7.bla()
				else{
					ctxt.compile(gAttr->getObjectExpression());
					ctxt.addInstruction(Instruction::createDup());
					ctxt.addInstruction(Instruction::createGetAttribute(attrId));
					break;
				}
			}else{
				ctxt.addInstruction(Instruction::createPushVoid());
				ctxt.compile(self->getGetFunctionExpression());
				break;
			}
			
		}while(false);
		for(std::vector<ObjRef>::const_iterator it=self->getParams().begin();it!=self->getParams().end();++it){
			ctxt.compile(*it);
		}
		ctxt.addInstruction(Instruction::createCall(self->getParams().size()));
	})
		
	// GetAttributeExpr
	ADD_HANDLER( _TypeIds::TYPE_GET_ATTRIBUTE_EXPRESSION, GetAttributeExpr, {
		if(self->getObjectExpression().isNotNull()){
			ctxt.compile(self->getObjectExpression());
			ctxt.addInstruction(Instruction::createGetAttribute(self->getAttrId()));
		}else{
			const int localVarIndex = ctxt.getCurrentVarIndex(self->getAttrId());
			if(localVarIndex>=0){
				ctxt.addInstruction(Instruction::createGetLocalVariable(localVarIndex));
			}else{
				ctxt.addInstruction(Instruction::createGetVariable(self->getAttrId()));
			}
		}

	})

	// IfStatement
	ADD_HANDLER( _TypeIds::TYPE_IF_STATEMENT, IfStatement, {
		if(self->getCondition().isNull()){
			if(self->getElseAction().isValid()){
				self->getElseAction()._asm(ctxt);
			}
		}else{
			const uint32_t elseMarker = ctxt.createMarker();
		
			ctxt.compile(self->getCondition());
			ctxt.addInstruction(Instruction::createJmpOnFalse(elseMarker));
			if(self->getAction().isValid()){
				self->getAction()._asm(ctxt);
			}
			
			if(self->getElseAction().isValid()){
				const uint32_t endMarker = ctxt.createMarker();
				ctxt.addInstruction(Instruction::createJmp(endMarker));
				ctxt.addInstruction(Instruction::createSetMarker(elseMarker));
				self->getElseAction()._asm(ctxt);
				ctxt.addInstruction(Instruction::createSetMarker(endMarker));
			}else{
				ctxt.addInstruction(Instruction::createSetMarker(elseMarker));
			}
		}
	})
	
	
	// IfStatement
	ADD_HANDLER( _TypeIds::TYPE_LOGIC_OP_EXPRESSION, LogicOpExpr, {
		switch(self->getOperator()){
			case LogicOpExpr::NOT:{
				ctxt.compile(self->getLeft());
				ctxt.addInstruction(Instruction::createNot());
				break;
			}
			case LogicOpExpr::OR:{
				const uint32_t marker = ctxt.createMarker();
				const uint32_t endMarker = ctxt.createMarker();
				ctxt.compile(self->getLeft());
				ctxt.addInstruction(Instruction::createJmpOnTrue(marker));
				ctxt.compile(self->getRight());
				ctxt.addInstruction(Instruction::createJmpOnTrue(marker));
				ctxt.addInstruction(Instruction::createPushBool(false));
				ctxt.addInstruction(Instruction::createJmp(endMarker));
				ctxt.addInstruction(Instruction::createSetMarker(marker));
				ctxt.addInstruction(Instruction::createPushBool(true));
				ctxt.addInstruction(Instruction::createSetMarker(endMarker));
				break;
			}
			default:
			case LogicOpExpr::AND:{
				const uint32_t marker = ctxt.createMarker();
				const uint32_t endMarker = ctxt.createMarker();
				ctxt.compile(self->getLeft());
				ctxt.addInstruction(Instruction::createJmpOnFalse(marker));
				ctxt.compile(self->getRight());
				ctxt.addInstruction(Instruction::createJmpOnFalse(marker));
				ctxt.addInstruction(Instruction::createPushBool(true));
				ctxt.addInstruction(Instruction::createJmp(endMarker));
				ctxt.addInstruction(Instruction::createSetMarker(marker));
				ctxt.addInstruction(Instruction::createPushBool(false));
				ctxt.addInstruction(Instruction::createSetMarker(endMarker));
				break;
			}
		}
	})
	
	// IfStatement
	ADD_HANDLER( _TypeIds::TYPE_LOOP_STATEMENT, LoopStatement, {
		const uint32_t loopBegin = ctxt.createMarker();
		const uint32_t loopEndMarker = ctxt.createMarker();
		const uint32_t loopContinueMarker = ctxt.createMarker();
		
		if(self->getInitStatement().isValid()){
			ctxt.setLine(self->getInitStatement().getLine());
			self->getInitStatement()._asm(ctxt);
		}
		ctxt.addInstruction(Instruction::createSetMarker(loopBegin));
		
		if(self->getPreConditionExpression().isNotNull()){
			ctxt.compile(self->getPreConditionExpression());
			ctxt.addInstruction(Instruction::createJmpOnFalse(loopEndMarker));
		}
		ctxt.pushSetting_marker( CompilerContext::BREAK_MARKER ,loopEndMarker);
		ctxt.pushSetting_marker( CompilerContext::CONTINUE_MARKER ,loopContinueMarker);
		self->getAction()._asm(ctxt);
		ctxt.popSetting();
		ctxt.popSetting();
		
		if(self->getPostConditionExpression().isNotNull()){ // increaseStmt is ignored!
			ctxt.addInstruction(Instruction::createSetMarker(loopContinueMarker));
			ctxt.compile(self->getPostConditionExpression());
			ctxt.addInstruction(Instruction::createJmpOnTrue(loopBegin));
		}else{
			ctxt.addInstruction(Instruction::createSetMarker(loopContinueMarker));
			if(self->getIncreaseStatement().isValid()){
				self->getIncreaseStatement()._asm(ctxt);
			}
			ctxt.addInstruction(Instruction::createJmp(loopBegin));
		}
		ctxt.addInstruction(Instruction::createSetMarker(loopEndMarker));
	})

	
	// IfStatement
	ADD_HANDLER( _TypeIds::TYPE_SET_ATTRIBUTE_EXPRESSION, SetAttributeExpr, {
		ctxt.compile(self->getValueExpression());

		ctxt.setLine(self->getLine());
		ctxt.addInstruction(Instruction::createDup());
		
		const StringId attrId = self->getAttrId();
		if(self->isAssignment()){
			// no object given: a = ...
			if(self->getObjectExpression().isNull()){
				// local variable: var a = ...	
				if(ctxt.getCurrentVarIndex(attrId)>=0){
					ctxt.addInstruction(Instruction::createAssignLocal(ctxt.getCurrentVarIndex(attrId)));
				}else{
					ctxt.addInstruction(Instruction::createAssignVariable(attrId));
				}
			}else{ // object.a = 
				ctxt.compile(self->getObjectExpression());
				ctxt.addInstruction(Instruction::createAssignAttribute(attrId));
			}
			
		}else{
				ctxt.compile(self->getObjectExpression());
				ctxt.addInstruction(Instruction::createPushUInt(static_cast<uint32_t>(self->getAttributeProperties())));
				ctxt.addInstruction(Instruction::createSetAttribute(attrId));
		}
	})

	// TryCatchStatement
	ADD_HANDLER( _TypeIds::TYPE_TRY_CATCH_STATEMENT, TryCatchStatement, {
		const uint32_t catchMarker = ctxt.createMarker();
		const uint32_t endMarker = ctxt.createMarker();
		
		// try
		// ------
		ctxt.pushSetting_marker(CompilerContext::EXCEPTION_MARKER,catchMarker);
		ctxt.addInstruction(Instruction::createSetExceptionHandler(catchMarker)); 

		// collect all variables that are declared inside the try-block (excluding nested try-blocks)
		std::vector<size_t> collectedVariableIndices;
		ctxt.pushLocalVarsCollector(&collectedVariableIndices);
		ctxt.compile(self->getTryBlock().get());
		ctxt.popLocalVarsCollector();

		ctxt.addInstruction(Instruction::createJmp(endMarker));
		ctxt.popSetting(); // EXCEPTION_MARKER
		
		// catch
		// ------
		const StringId exceptionVariableName = self->getExceptionVariableName();
		
		ctxt.addInstruction(Instruction::createSetMarker(catchMarker));
		// reset catchMarker
		ctxt.addInstruction(Instruction::createSetExceptionHandler(ctxt.getCurrentMarker(CompilerContext::EXCEPTION_MARKER))); 

		// clear all variables defined inside try block
		for(std::vector<size_t>::const_iterator it = collectedVariableIndices.begin(); it!=collectedVariableIndices.end();++it){
			ctxt.addInstruction(Instruction::createResetLocalVariable(*it));
		}
		
		// define exception variable
		if(!exceptionVariableName.empty()){
			std::set<StringId> varSet;
			varSet.insert(exceptionVariableName);
			ctxt.pushSetting_localVars(varSet);
		}
		// load exception variable with exception object ( exceptionVariableName = __result )
		ctxt.addInstruction(Instruction::createGetLocalVariable(2));  
		ctxt.addInstruction(Instruction::createAssignLocal(ctxt.getCurrentVarIndex(exceptionVariableName))); 
		
		// execute catch block
		ctxt.compile(self->getCatchBlock().get());
		// pop exception variable
		if(!exceptionVariableName.empty()){
			ctxt.addInstruction(Instruction::createResetLocalVariable(ctxt.getCurrentVarIndex(exceptionVariableName)));
			ctxt.popSetting(); // variable
		}
		// end:
		ctxt.addInstruction(Instruction::createSetMarker(endMarker));
	})

	// ------------------------
	// Other objects
	ADD_HANDLER( _TypeIds::TYPE_USER_FUNCTION, UserFunction, {
		// compiling the function itself
		if(ctxt.isCurrentInstructionBlock(self->getInstructions())){
			
			ctxt.pushSetting_basicLocalVars(); // make 'this' and parameters available
			ctxt.compile(self->getBlock());
			ctxt.popSetting();
			CompilerContext::finalizeInstructions(self->getInstructions());
		}else{
			Compiler c;
			CompilerContext ctxt2(c,self->getInstructions());
			ctxt2.pushSetting_basicLocalVars(); // make 'this' and parameters available
			ctxt2.compile(self);
			ctxt2.popSetting();
			CompilerContext::finalizeInstructions(self->getInstructions());
			
			
			ctxt.addInstruction(Instruction::createPushFunction(ctxt.registerInternalFunction(self))); 
		}
			
	})
		
	// ------------------------
	#undef ADD_HANDLER
	return true;
}



}