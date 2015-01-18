// Type.cpp
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2011-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#include "Type.h"

#include "../Basics.h"
#include "../StdObjects.h"
#include "Identifier.h"
#include "Exception.h"
#include "../Utils/SyncTools.h"

namespace EScript{

//! (static)
Type * Type::getTypeObject(){
	struct factory{ // use factory for static one time initialization
		Type * operator()(){
			// This object defines the type of all 'Type' objects.
			// It inherits from Object and the type of the object is defined by itthisObj.
			Type * typeObject = new Type(Object::getTypeObject(),nullptr);
			typeObject->typeRef = typeObject;
			return typeObject;
		}
	};

	static Type * typeObject = factory()();
	return typeObject;
}

//! initMembers
void Type::init(EScript::Namespace & globals) {
	// [Type] ---|> [Object]
	Type * typeObject = getTypeObject();
	initPrintableName(typeObject,getClassName());

	declareConstant(&globals,getClassName(),typeObject);

	//! [ESMF] Type new Type( [BaseType = ExtObject] )
	ES_CONSTRUCTOR(typeObject,0,1,{
		Type * baseType = parameter.count() == 0 ? ExtObject::getTypeObject() : assertType<Type>(rt,parameter[0]);
		if(!baseType->allowsUserInheritance()){
			rt.setException("Basetype '"+baseType->toString()+"' does not allow user inheritance.");
			return nullptr;
		}
		Type * newType = new Type(baseType);
		newType->allowUserInheritance(true); // user defined Types allow user inheritance per default.
		return newType;
	})


	//! [ESMF] Type Type.getBaseType()
	ES_MFUN(typeObject,const Type,"getBaseType",0,0, thisObj->getBaseType())

	//! [ESMF] Map Type.getObjAttributes()
	ES_MFUN(typeObject,const Type,"getObjAttributes",0,0,
		Map::create(thisObj->collectObjAttributes()))

	//! [ESMF] Map Type.getTypeAttributes()
	ES_MFUN(typeObject,const Type,"getTypeAttributes",0,0,
		Map::create(thisObj->collectTypeAttributes()))

	//! [ESMF] Type Type.hasBase(Type)
	ES_MFUN(typeObject,const Type,"hasBase",1,1, thisObj->hasBase(parameter[0].to<Type*>(rt)))

	//! [ESMF] Type Type.isBaseOf(Type)
	ES_MFUN(typeObject,const Type,"isBaseOf",1,1, thisObj->isBaseOf(parameter[0].to<Type*>(rt)))
}

//---

//! (ctor)
Type::Type():
	Object(Type::getTypeObject()),flags(0),baseType(Object::getTypeObject()) {
	//ctor
}

//! (ctor)
Type::Type(Type * _baseType):
		Object(Type::getTypeObject()),flags(0),baseType(_baseType) {

	if(getBaseType()!=nullptr)
		getBaseType()->copyObjAttributesTo(this);
	//ctor
}

//! (ctor)
Type::Type(Type * _baseType,Type * typeOfType):
		Object(typeOfType),flags(0),baseType(_baseType) {

	if(getBaseType()!=nullptr)
		getBaseType()->copyObjAttributesTo(this);
	//ctor
}

//! (dtor)
Type::~Type() {
	//dtor
}

//! ---|> [Object]
Object * Type::clone() const{
	return new Type(getBaseType(),getType());
}

static const char * typeAttrErrorHint =
	"This may be a result of: Adding object attributes to a Type AFTER inheriting from that Type, "
	"adding object attributes to a Type AFTER creating instances of that Type, "
	"or adding object attributes to a Type whose instances cannot store object attributes. ";


Object::AttributeReference_t Type::findTypeAttribute(const StringId & id){
	Type * t = this;
	do{
		{
#if defined(ES_THREADING)
			SyncTools::MutexHolder dbLock(t->attributesMutex);
#endif // ES_THREADING
			Attribute * attr = t->attributes.accessAttribute(id);
			if( attr ){
				if( attr->isObjAttribute() ){
					std::string message = "(findTypeAttribute) type-attribute expected but object-attribute found. ('";
					message += id.toString() + "')\n" + typeAttrErrorHint;
					throw new Exception(message);
				}
#if defined(ES_THREADING)
				return std::make_tuple(attr,std::move(dbLock));
#else
				return std::make_tuple(attr);
#endif // ES_THREADING
			}
		}
		t = t->getBaseType();
	}while(t);
#if defined(ES_THREADING)
	return std::make_tuple(nullptr,SyncTools::MutexHolder());
#else
	return std::make_tuple(nullptr);
#endif // ES_THREADING
}


//! ---|> Object
Object::AttributeReference_t Type::_accessAttribute(const StringId & id,bool localOnly){
	// is local attribute?
#if defined(ES_THREADING)
	{
		SyncTools::MutexHolder mutexHolder( attributesMutex );
		Attribute * attr = attributes.accessAttribute(id);
		if( attr )
			return std::move(std::make_tuple(attr,std::move(mutexHolder)));
	}
	if(localOnly)
		return std::move(std::make_tuple(nullptr,SyncTools::MutexHolder()));
#else
	{
		Attribute* const attr = attributes.accessAttribute(id);
		if(attr || localOnly)
			return std::make_tuple(attr);
	}
#endif

	// try to find the attribute along the inherited path...
	if(getBaseType()){
		AttributeReference_t attrRef( std::move( getBaseType()->findTypeAttribute(id) ));
		if(std::get<0>(attrRef))
			return std::move(attrRef);
	}

	// try to find the attribute from this type's type.
	if(getType())
		return std::move( getType()->findTypeAttribute(id));
#if defined(ES_THREADING)
	return std::move(std::make_tuple(nullptr,SyncTools::MutexHolder()));
#else
	return std::move(std::make_tuple(nullptr));
#endif
//	return std::make_tuple(getType()!=nullptr ? getType()->findTypeAttribute(id) : nullptr);
}

//! ---|> Object
bool Type::setAttribute(const StringId & id,const Attribute & attr){
	attributes.setAttribute(id,attr);
	if(attr.isObjAttribute())
		setFlag(FLAG_CONTAINS_OBJ_ATTRS,true);
	return true;
}

void Type::copyObjAttributesTo(Object * instance){
	// init member vars of type
	if(getFlag(FLAG_CONTAINS_OBJ_ATTRS)){
		for(const auto & keyValuePair : attributes) {
			const Attribute & a = keyValuePair.second;
			if( a.isNull() || a.isTypeAttribute() )
				continue;
			instance->setAttribute(keyValuePair.first, Attribute(std::move(a.getValue()->getRefOrCopy()),a.getProperties()));
		}
	}
}

std::unordered_map<StringId,ObjRef> Type::collectTypeAttributes()const{
	std::unordered_map<StringId,ObjRef> attrs;
	for(const auto & keyValuePair : attributes) {
		if(keyValuePair.second.isTypeAttribute()) 
			attrs[keyValuePair.first] = keyValuePair.second.getValue();
	}
	return std::move(attrs);
}

std::unordered_map<StringId,ObjRef> Type::collectObjAttributes()const{
	std::unordered_map<StringId,ObjRef> attrs;
	for(const auto & keyValuePair : attributes) {
		if(keyValuePair.second.isObjAttribute())
			attrs[keyValuePair.first] = keyValuePair.second.getValue();
	}
	return std::move(attrs);
}

//! ---|> Object
std::unordered_map<StringId,ObjRef> Type::collectLocalAttributes(){
	std::unordered_map<StringId,ObjRef> attrs;
	for(const auto & keyValuePair : attributes)
		attrs[keyValuePair.first] = keyValuePair.second.getValue();
	return std::move(attrs);
}

bool Type::hasBase(const Type * type) const {
	for(const Type * t = this; t; t = t->getBaseType()){
		if(t==type)
			return true;
	}
	return false;
}

bool Type::isBaseOf(const Type * type) const {
	while(type){
		if(type==this)
			return true;
		type = type->getBaseType();
	}
	return false;
}


}
