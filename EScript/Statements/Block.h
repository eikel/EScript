// Block.h
// This file is part of the EScript programming language.
// See copyright notice in EScript.h
// ------------------------------------------------------
#ifndef BLOCK_H
#define BLOCK_H

#include "../Object.h"

#include <list>
#include <deque>
#include <set>

namespace EScript {

/*! [Block]  ---|> [Object] */
class Block : public Object {
		ES_PROVIDES_TYPE_NAME(BlockStatement)
	public:
		typedef std::deque<Object *> statementList;
		typedef statementList::iterator statementCursor;
		typedef statementList::const_iterator cStatementCursor;
		typedef std::set<identifierId>  declaredVariableMap_t;

		Block(int lineNr=-1);
		virtual ~Block();

		statementList & getStatements()                 {   return statements;  }
		void setFilename(identifierId filename)  		{   filenameId=filename;  }
		std::string getFilename()const                  {   return identifierIdToString(filenameId);    }
		int getLine()const								{	return line;	}


		/*! returns false if variable was already declared */
		bool declareVar(const std::string & s)			{	return declareVar(EScript::stringToIdentifierId(s));	}
		bool declareVar(identifierId id);
		const declaredVariableMap_t * getVars()const 	{ 	return vars;    }
		bool isLocalVar(identifierId id)				{	return vars==NULL ? false : vars->count(id)>0;	}
		void addStatement(Object * s);
		bool hasLocalVars()const						{	return vars!=NULL && !vars->empty(); }

		/// ---|> [Object]
		virtual std::string toString()const ;
		virtual Object * execute(Runtime & rt);

	private:
		identifierId filenameId; // for debugging
		declaredVariableMap_t * vars;
		statementList statements;
		int line;
};
}

#endif // BLOCK_H
