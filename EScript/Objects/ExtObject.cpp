// ExtObject.cpp
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2011-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#include "ExtObject.h"

#include "../Basics.h"
#include "../StdObjects.h"

namespace EScript{

//! (static)
Type * ExtObject::getTypeObject(){
	static Type * typeObject = new Type(Object::getTypeObject()); // ---|> Object
	return typeObject;
}

//! (static) initMembers
void ExtObject::init(EScript::Namespace & globals) {
	Type * typeObject = getTypeObject();
	typeObject->allowUserInheritance(true);
	initPrintableName(typeObject,getClassName());

	declareConstant(&globals,getClassName(),typeObject);

	//!	[ESF] ExtObject new ExtObject( [Map objAttributes] )
	ES_CONSTRUCTOR(typeObject,0,1,{
		ERef<ExtObject> result(new ExtObject(thisType));
		if(parameter.count()>0){
			Map * m = assertType<Map>(rt,parameter[0]);
			for(const auto & keyValuePair : *m) {
				result->setAttribute(keyValuePair.first, Attribute(keyValuePair.second.value));
			}
		}
		return result.detachAndDecrease();
	})

}

// -----------------------------------------------------------------------------------------------

//! (static) factory
ExtObject * ExtObject::create(){
	return new ExtObject;
}


//! (ctor)
ExtObject::ExtObject() : Object(ExtObject::getTypeObject()) {
	//ctor
}

ExtObject::ExtObject(const ExtObject & other): Object(other.getType()){
//	if(typeRef)
//		typeRef->copyObjAttributesTo(this);
	cloneAttributesFrom(&other);
}

//! (ctor)
ExtObject::ExtObject(Type * type) : Object(type) {
	if(typeRef)
		typeRef->copyObjAttributesTo(this);
	//ctor
}


//! ---|> [Object]
Object * ExtObject::clone() const{
	ExtObject * c = new ExtObject(getType());
	c->cloneAttributesFrom(this);
	return c;
}

// -----------------------------------------------------------------------------------------------
// attributes

//! ---|> [Object]
void ExtObject::_initAttributes(Runtime & rt){
	objAttributes.initAttributes(rt);
}


//! ---|> [Object]
Object::AttributeReference_t ExtObject::_accessAttribute(const StringId & id,bool localOnly){
#if defined(ES_THREADING)
	{
		SyncTools::FastLockHolder mutexHolder( attributesMutex );
		Attribute * attr = objAttributes.accessAttribute(id);
		if( attr )
			return std::move(std::make_tuple(attr,std::move(mutexHolder)));
	}
	if(localOnly || !getType() )
		return std::move(std::make_tuple(nullptr,SyncTools::FastLockHolder()));
	else 
		return std::move(getType()->findTypeAttribute(id));
#else
	Attribute * attr = objAttributes.accessAttribute(id);
	if( attr || localOnly || !getType() )
		return std::make_tuple(attr);
	else 
		return std::move(getType()->findTypeAttribute(id));
#endif

}

//! ---|> [Object]
bool ExtObject::setAttribute(const StringId & id,const Attribute & attr){
#if defined(ES_THREADING)
	SyncTools::FastLockHolder mutexHolder( attributesMutex );
#endif
	objAttributes.setAttribute(id,attr);
	return true;
}

void ExtObject::cloneAttributesFrom(const ExtObject * obj) {
#if defined(ES_THREADING)
	SyncTools::FastLockHolder mutexHolder( attributesMutex );
#endif
	objAttributes.cloneAttributesFrom(obj->objAttributes);
}

//! ---|> Object
std::unordered_map<StringId,ObjRef> ExtObject::collectLocalAttributes(){
#if defined(ES_THREADING)
	SyncTools::FastLockHolder mutexHolder( attributesMutex );
#endif
	return std::move(objAttributes.collectAttributes());
}
}
