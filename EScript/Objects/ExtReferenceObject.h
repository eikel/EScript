// ExtReferenceObject.h
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2012-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012-2013 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#ifndef EXT_REFERENCE_OBJECT_H
#define EXT_REFERENCE_OBJECT_H

#include "../Utils/AttributeContainer.h"
#include "ReferenceObject.h"
#include "Type.h"

namespace EScript {


namespace Policies{

/*! Policy for locating an ExtRefernceObject's attribute storage.
	Use this policy to directly store the AttributeContainer inside the ExtReferenceObject.
	Alternative implementations could e.g. store the attributeContainer as user data at the referenced object. */
class StoreAttrsInEObject_Policy{
		AttributeContainer attributeContainer;
	protected:
		/*!	(static) Returns a pointer to the object's attributeContainer. If @param create is 'false' and
			the object has no attributeContainer, the function returns nullptr. If @param create is 'true' and no
			attributeContainer exists, a new one is created, so that always an valid container is returned. */
		static AttributeContainer * getAttributeContainer(StoreAttrsInEObject_Policy * obj,bool /*create*/){
			return &(obj->attributeContainer);
		}

		/*! (static) Should return true iff the object's Type's attributes are already initialized with the
			object's attributeContainer. This function is only called by the ExtReferenceObject's constructor.
			As for this specific policy, the attributeContainer has always just been created then, it can not already
			been initialized. */
		static bool areObjAttributesInitialized(StoreAttrsInEObject_Policy * /*obj*/){
			return false;
		}
};

/*! Use one common mutex for all objects of a common type. This is not very efficient when there is massively 
	concurrent access to those objects, but requires no per-object memory overhead.	*/
template<class T>
class TypeBasedAttributeLocking_Policy{
	protected:
#if defined(ES_THREADING)
		static SyncTools::FastLockHolder aquireAttributeLock(){
			static SyncTools::FastLock attrMutex;
			return SyncTools::FastLockHolder(attrMutex);
		}
#else
		static bool aquireAttributeLock(){	return true;	} // dummy
#endif

};

}

/*! [ExtReferenceObject] ---|> [Object]
	A Ext(entable)ReferenceObject can be used as wrapper for user defined C++ objects that can be enriched by user
	defined attributes. For a description how the C++-object is handled and how the comparisonPolicy works, \see ReferenceObject.h
	The way the AttributeContainer is stored is controlled by the @tparam attributeProvider.
*/
template <typename _T,
		typename comparisonPolicy = Policies::EqualContent_ComparePolicy, 
		typename attributeProvider = Policies::StoreAttrsInEObject_Policy,
		typename attributeLockProvider = Policies::TypeBasedAttributeLocking_Policy<_T>>
class ExtReferenceObject : public Object, private attributeProvider, private attributeLockProvider{
		ES_PROVIDES_TYPE_NAME(ExtReferenceObject)
	public:
		typedef ExtReferenceObject<_T,comparisonPolicy,attributeProvider,attributeLockProvider> ExtReferenceObject_t;

		// ---
		//! (ctor)
		ExtReferenceObject(const _T & _obj, Type * type = nullptr) :
					Object(type), attributeProvider(), obj(_obj){
			if(type!=nullptr && !attributeProvider::areObjAttributesInitialized(this))
				type->copyObjAttributesTo(this);
		}
		//! (ctor) Passes arbitrary parameters to the object's constructor.
		template<typename ...args>
		explicit ExtReferenceObject(Type * type,args&&... params) :
				Object(type), obj(std::forward<args>(params)...) {
			if(type!=nullptr && !attributeProvider::areObjAttributesInitialized(this))
				type->copyObjAttributesTo(this);
		}

		virtual ~ExtReferenceObject()						{	}


		/*! ---|> [Object]
			Direct cloning of a ExtReferenceObject is forbidden; but you may override the clone function in the specific implementation */
		ExtReferenceObject_t * clone()const override {
			throwRuntimeException("Trying to clone unclonable object '"+this->toString()+"'");
			return nullptr;
		}
		//! ---|> [Object]
		bool rt_isEqual(Runtime &,const ObjPtr & o) override	{	return comparisonPolicy::isEqual(this,o);	}


	// -----

	//! @name Reference
	//	@{
	public:
		inline const _T & ref() const			{	return obj;	}
		inline _T & ref() 						{	return obj;	}
		inline const _T & operator*()const		{	return obj;	}
		inline _T & operator*()					{	return obj;	}

	private:
		_T obj;
	//	@}

	// -----

	//! @name Attributes
	//	@{
	public:
		using attributeProvider::getAttributeContainer;
		using attributeLockProvider::aquireAttributeLock;
		using Object::_initAttributes;
		using Object::_accessAttribute;
		using Object::setAttribute;

		//! ---|> [Object]
		AttributeReference_t _accessAttribute(const StringId & id,bool localOnly) override{
			auto attrLock = aquireAttributeLock();
			AttributeContainer * attrContainer = getAttributeContainer(this,false);
			Attribute * attr = attrContainer!=nullptr ? attrContainer->accessAttribute(id) : nullptr;
			return  ( attr!=nullptr || localOnly || getType()==nullptr) ? attr : getType()->findTypeAttribute(id);
		}

		//! ---|> [Object]
		void _initAttributes(Runtime & rt) override{
			// if the type contains obj attributes, this object will surely also have some, so it is safe to init the attribute container.
			if(getType()!=nullptr && getType()->getFlag(Type::FLAG_CONTAINS_OBJ_ATTRS) ){
				getAttributeContainer(this,true)->initAttributes(rt);
			}
		}

		//! ---|> [Object]
		bool setAttribute(const StringId & id,const Attribute & attr) override{
			auto attrLock = aquireAttributeLock();
			getAttributeContainer(this,true)->setAttribute(id,attr);
			return true;
		}

		//! ---|> [Object]
		std::unordered_map<StringId,ObjRef> collectLocalAttributes() override{
			auto attrLock = aquireAttributeLock();
			AttributeContainer * attrContainer = getAttributeContainer(this,false);
			if(attrContainer)
				return std::move(attrContainer->collectAttributes());
			return std::unordered_map<StringId,ObjRef>();
		}
	// @}

};

}

#endif // EXT_REFERENCE_OBJECT_H
