// Attribute.h
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2012-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#ifndef ES_Attribute_H
#define ES_Attribute_H

#include "ObjRef.h"
#include "StringId.h"
#include <cstdint>

namespace EScript {

//! Internal representation of an object's attribute
class Attribute{
	public:
		typedef uint8_t flag_t;
		static const flag_t NORMAL_ATTRIBUTE = 0;
		static const flag_t CONST_BIT = (1<<0);	// 0...normal	1...const
		static const flag_t PRIVATE_BIT = (1<<1);	// 0...public	1...private
		static const flag_t TYPE_ATTR_BIT = (1<<2);	// 0...objAttr	1...typeAttr
		static const flag_t INIT_BIT = (1<<3);		// 0...normal	1...init
		static const flag_t OVERRIDE_BIT = (1<<4);	// 0...normal	1...override

		static const flag_t ASSIGNMENT_RELEVANT_BITS = CONST_BIT|PRIVATE_BIT;

	private:
		ObjRef value;
		flag_t properties;
	public:
		explicit Attribute(flag_t _properties = NORMAL_ATTRIBUTE):properties(_properties) {}
		/*implicit*/ Attribute(const ObjPtr & _value,flag_t  _properties = NORMAL_ATTRIBUTE):value(_value.get()),properties(_properties) {}
		/*implicit*/ Attribute(const ObjRef & _value,flag_t  _properties = NORMAL_ATTRIBUTE):value(_value.get()),properties(_properties) {}
		/*implicit*/ Attribute(Object * _value,flag_t  _properties = NORMAL_ATTRIBUTE):value(_value),properties(_properties) {}
		/*implicit*/ Attribute(const Attribute & e):value(e.value),properties(e.properties) {}
		/*implicit*/ Attribute(Attribute &&) = default;

		bool getProperty(flag_t f)const	{	return (properties&f)>0;	}
		flag_t getProperties()const		{	return properties;	}

		const ObjRef& getValue()const	{	return value;	}
		ObjRef extractValue()			{	return std::move(value);	}
		bool isConst()const				{	return properties&CONST_BIT;	}
		bool isInitializable()const		{	return properties&INIT_BIT;	}
		bool isNull()const				{	return value.isNull();	}
		bool isNotNull()const			{	return value.isNotNull();	}
		explicit operator bool()const	{	return bool(value);	}
		bool isObjAttribute()const		{	return !(properties&TYPE_ATTR_BIT);	}
		bool isTypeAttribute()const		{	return properties&TYPE_ATTR_BIT;	}
		bool isPrivate()const			{	return properties&PRIVATE_BIT;	}
		bool isOverriding()const		{	return properties&OVERRIDE_BIT;	}

		void setValue(Object * v)		{	value = v;	}
		void set(Object * v,flag_t f)	{	value = v, properties = f;	}
		Attribute & operator=(const Attribute & e){
			set(e.value.get(), e.properties);
			return *this;
		}
		Attribute & operator=(Attribute && e){
			value = std::move(e.value);
			properties = e.properties;
			return *this;
		}
		Attribute & operator=(ObjRef && v){
			value = std::move(v);
			return *this;
		}

};
}
#endif // ES_Attribute_H
