// Type.h
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2011-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2011-2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#ifndef ES_Type_H
#define ES_Type_H

#include "Object.h"
#include "../Utils/AttributeContainer.h"

#if defined(ES_THREADING)
#include "../Utils/SyncTools.h"
#endif

#include <cstdint>

namespace EScript {

//! [Type] ---|> [Object]
class Type : public Object {
		ES_PROVIDES_TYPE_NAME(Type)

	// -------------------------------------------------------------

	//! @name Initialization
	//	@{
	public:
		static Type * getTypeObject();
		static void init(EScript::Namespace & globals);
	//	@}

	// -------------------------------------------------------------

	//! @name Main
	//	@{
	public:
		Type();
		Type(Type * baseType);
		Type(Type * baseType,Type * typeOfType);
		virtual ~Type();

		//! ---|> [Object]
		Object * clone() const override;
		internalTypeId_t _getInternalTypeId()const override	{	return _TypeIds::TYPE_TYPE;	}
	//	@}

	// -------------------------------------------------------------

	/*! @name Attributes
		\note for an information about the using directives, see
			http://publib.boulder.ibm.com/infocenter/lnxpcomp/v8v101/topic/com.ibm.xlcpp8l.doc/language/ref/overload_member_fn_base_derived.htm
	*/
	// @{
	public:

		//! Get only the typeAttributes.
		std::unordered_map<StringId,ObjRef> collectTypeAttributes()const;
		//! Get only the objectAttributes.
		std::unordered_map<StringId,ObjRef> collectObjAttributes()const;

		void copyObjAttributesTo(Object * instance);

		//! Used by instances of this type get the value of an inherited typeAttribute.
		AttributeReference_t findTypeAttribute(const StringId & id);

		using Object::_accessAttribute;
		using Object::setAttribute;

		//! ---|> [Object]
		AttributeReference_t _accessAttribute(const StringId & id,bool localOnly) override;

		//! ---|> [Object]
		bool setAttribute(const StringId & id,const Attribute & attr) override;

		//! ---|> [Object]
		std::unordered_map<StringId,ObjRef> collectLocalAttributes() override;

	private:
		AttributeContainer attributes;
	#if defined(ES_THREADING)
		mutable SyncTools::FastLock attributesMutex;
	#endif // ES_THREADING
	// @}

	// -------------------------------------------------------------

	//! @name Flags
	// @{
	public:
		typedef uint16_t flag_t;

		static const flag_t FLAG_CALL_BY_VALUE = 1<<0;
		static const flag_t FLAG_CONTAINS_OBJ_ATTRS = 1<<1;
		static const flag_t FLAG_ALLOWS_USER_INHERITANCE = 1<<2;

		bool getFlag(flag_t f)const				{	return (flags&f) >0;	}
		void setFlag(flag_t f,bool b = true)	{	b? flags|=f : flags-=(flags&f);	}
		flag_t getFlags()const					{	return flags;	}
	private:
		flag_t flags;
	// @}

	// -------------------------------------------------------------

	//! @name Inheritance
	//	@{
	public:
		void allowUserInheritance(bool b)			{	setFlag(FLAG_ALLOWS_USER_INHERITANCE,b);	}
		bool allowsUserInheritance()const			{	return getFlag(FLAG_ALLOWS_USER_INHERITANCE);	}

		Type * getBaseType()const					{	return baseType.get();	}

		bool hasBase(const Type * type)const;
		bool isBaseOf(const Type * type)const;

	private:
		ERef<Type> baseType;
	//	@}

};

}
#endif // ES_Type_H
