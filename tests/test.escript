// test.escript
// This file is part of the EScript programming language (https://github.com/EScript)
//
// Copyright (C) 2011-2015 Claudius Jähn <ClaudiusJ@live.de>
// Copyright (C) 2012 Benjamin Eikel <benjamin@eikel.org>
//
// Licensed under the MIT License. See LICENSE file for details.
// ---------------------------------------------------------------------------------
outln ("EScript Testcases\nVersion: ",EScript.VERSION_STRING,"\n","-"*79);

/* Threading
Thread safe:
 - StringId
 - ExtObject
 - Type
 - Namespace
 - ? Array
 - ? Map
 - @(once)
 - ObjPtr RuntimeInternals::getGlobalVariable
 - ? startFunctionExecution should keep reference to function?
 - ? runtime.executeFunction should keep reference to function?
 - ? runtimeHelper callXXX should keep reference to function?
 - loadOnce
Objects:
 - PooledMutex
 - Thread
 - Mutex
 - Lock
*/
//
//var f = fn(){
//
//	while(true)
//		out("foo");
//};
//outln( f._asm() );

var thread1 = Threading.run( ["foo"] => fn(t){while(true){out(t);}});
var thread2 = Threading.run( ["bar"] => fn(t){while(true){out(t);}});
//var thread2 = Threading.run( f );
//var thread2 = Threading.run( fn(){
//				while(true){
//								out;
////								2;
////								outln(1);
////					out("bar");
//				}
//				
//			});
//while(true);
//
//var rt0 = Runtime._getActiveRuntime();
//rt0.id := 0;
//var rt1 = rt0._fork();
//rt1.id := 1;
//outln("rt0: ",rt0);
//outln("rt1: ",rt1);
//
//
//var f = fn(){
//	var rt = Runtime._getActiveRuntime();
//	outln("rtX: ",rt);
//	outln(rt.id);
//};
//f();
//rt1._callFunction(f);
////outln(rt1.id);
//outln(Threading);



//----
// init
GLOBALS.benchmark:=false;
GLOBALS.errors:=0; // error count

//! new testing function
GLOBALS.test := fn(description,result,checkCoverageType=false){
	if(!result)
		++errors;
	if(!benchmark){
		out(description.fillUp(25," "),result?"ok":"failed");

		if(checkCoverageType){
			var numFunctions = 0;
			var coveredFunctions = 0;
			var missing = [];
			foreach(checkCoverageType._getAttributes() as var fun){
				if(! (fun---|>Function))
					continue;
				++numFunctions;
				if(fun._getCallCounter()>0){
					++coveredFunctions;
				}else{
					missing+=fun.getOriginalName();
				}
			}
			out("\t (",coveredFunctions,"/",numFunctions,")");
			if(!missing.empty()){
				out("\nMissing: ",missing.implode(", "));
			}
		}
		out("\n");

	}

};

(fn(){})._asm(); // ... to mark _asm as executed.

var start = clock();
var outBackup = out;
addSearchPath(__DIR__);

//----
var t = load("Testcases_Core.escript");

if(benchmark){
	var progress = fn( percent ){
		var i = (percent*20).floor();
		SGLOBALS.out("\r","|"+"="*i+"|"+" "*(20-i)+"| "+percent.round(0.01)*100+"%	");
	};

	GLOBALS.out:=fn(values*){ ;};
	var times = [];
	var tries = 200;
	var innerLoops = 20;
	var sum = 0;

	Runtime._setAddStackInfoToExceptions(false); // disable costly stack infos

	progress(0);

	for(var i = 0;i<tries;++i){
		var startTime = clock();
		for(var j = 0;j<innerLoops;++j){
			t();
		}
		var time = (clock()-startTime)*1000;
		sum += time;
		times += (time/innerLoops).round(0.01); // ms per execution
		progress(i/tries);
	}
	progress(1);
	Runtime._setAddStackInfoToExceptions(true);

//	print_r(times);
	times.sort();
	SGLOBALS.out("\nMin:",times.front(),"ms\tMed:",times[ (times.count()*0.5).floor() ],"ms\tMax:",times.back(),"ms\n" );
	SGLOBALS.out("Avg:",sum/ (tries*innerLoops),"\n");
}else{
	try{
		t();
	}catch(e){
		Runtime.log(Runtime.LOG_ERROR,e);
		++errors;
	}
}

load("Testcases_IOLib.escript");
load("Testcases_MathLib.escript");
load("Testcases_Runtime.escript");
load("Testcases_StdLib.escript");
//if(getOS()=="WINDOWS")
//	load(__DIR__+"/Testcases_Win32Lib.escript");
load("Bugs.escript");

GLOBALS.out = outBackup;

out("\n-----\n");
if(errors>0)
	out("Errors:\t\t",errors,"!\n");
else
	out("No Errors.\n");

out("Duration:\t",clock()-start," sec\n");

return "Bye!";
// ----------------------------------------------------------
