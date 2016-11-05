// DataWrapper.escript
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2015 Claudius Jähn <ClaudiusJ@live.de>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------

var N = new Namespace;

//Threading.run( ["foo"] => fn(t){while(testRunning){out(t);}});
static AsyncFuture = new Type;
AsyncFuture.mResultContainer @(private,init) := Array;
AsyncFuture.mExceptionContainer @(private,init) := Array;
//AsyncFuture.mMutex @(private,init) := Threading.Mutex;

AsyncFuture.get ::= fn(){
	this.wait();
	return this.mResultContainer.popFront();
//	var result;
//	{
//		var theLock = new Threading.LockGuard( this.mMutex );
//		result = this.mResultContainer.popFront();
//	}
//	return result;
};
AsyncFuture.wait ::= fn(){
	if(this.mThread){
		this.mThread.join();
		this.mThread = void;
	}
	if(!this.mExceptionContainer.empty())
		throw this.mExceptionContainer.popFront();
	return this;
};
AsyncFuture._constructor ::= fn( inCallable ){
	this.mThread @(private) := Threading.run( [inCallable,mResultContainer,mExceptionContainer]=>
											 fn(inCallable,inResultContainer,inExceptionContainer){
		try{
			inResultContainer += inCallable();
//			var theResult = inCallable();
//			{
//				// lock
//			outln("+",__LINE__,);
//				var theLock = new Threading.LockGuard( inMutex );
//			outln("+",__LINE__);
//				inResultContainer += theResult;
////			outln("+",__LINE__);
//			}
		}catch(e){
				// lock
			print_r(e);
			//var theLock = new Threading.LockGuard( this.mMutex );
			inExceptionContainer += e;
		}
	});

};

//! \todo yield
N.async := fn( inCallable ){
	return new AsyncFuture( inCallable );
};

// ------


// generate
// TaskQueue
//

return N;
