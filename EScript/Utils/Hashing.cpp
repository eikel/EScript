// Hashing.cpp
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2011-2013 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2011-2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
#include "Hashing.h"
#if defined(ES_THREADING)
#include "SyncTools.h"
#endif

#include <cstddef>
#include <functional>
#include <map>
#include <utility>

namespace EScript{

typedef std::map<identifierId,std::string> identifierDB;

/**
 * (static) Returns the identifier database (and a mutex)
 * The database is created on the first call.
 * (When using a normal initializing, some compile order may cause runtime errors
 * if static identifiers are defined in other files that are compiled earlier.)
 */
#if defined(ES_THREADING)
static std::tuple<identifierDB,SyncTools::Mutex> & getIdentifierDB(){
	static std::tuple<identifierDB,SyncTools::Mutex> db;
	return db;
}
#else // no ES_THREADING
static std::tuple<identifierDB> & getIdentifierDB(){
	static std::tuple<identifierDB> db;
	return db;
}
#endif
const std::string ES_UNKNOWN_IDENTIFIER="[?]";

//! (internal)
hashvalue _hash( const std::string &  s) {
	hashvalue h = 0;
	for(size_t i = 0;i<s.length();++i)
		h^=(((s.at(i)+h)*1234393)% 0xffffff);
	return h;
}


identifierId stringToIdentifierId(const std::string & s){
	identifierId id = _hash(s);
	{
		auto & dbAndMutex = getIdentifierDB();
		auto & db = std::get<0>(dbAndMutex);
#if defined(ES_THREADING)
		SyncTools::MutexHolder dbLock(std::get<1>(dbAndMutex);
#endif // ES_THREADING
		while(true){
			identifierDB::iterator lbIt = db.lower_bound(id);
			if(lbIt==db.end() || db.key_comp()(id, lbIt->first) ){
				// id not found -> insert it
				db.insert(lbIt,std::make_pair(id,s));
				break;
			}else if( s==lbIt->second){
				// same string already inserted
				break;
			}else {
				// collision
				++id;
			}
		}
	}
	return id;
}

const std::string & identifierIdToString(identifierId id){
	auto & dbAndMutex = getIdentifierDB();
	auto & db = std::get<0>(dbAndMutex);
#if defined(ES_THREADING)
	SyncTools::MutexHolder dbLock(std::get<1>(dbAndMutex);
#endif // ES_THREADING
	const identifierDB::const_iterator it = db.find(id);
	if(it == db.end() )
		return ES_UNKNOWN_IDENTIFIER;
	else return (*it).second;
}
}
