#ifndef TIMER
#define TIMER

#include <chrono>
// no nesting, but makes for a very simple interface
	// could probably do something stack based, have Tick() push and Tock() pop
#define NOW std::chrono::high_resolution_clock::now()
#define USCAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()
static auto tInit = NOW;
static auto t = NOW;

// set base time
static inline void Tick () { t = NOW; }
// get difference between base time and current time, return value in useconds
static inline float Tock () { return USCAST( NOW - t ); }
// getting the time since the engine was started
static inline float TotalTime () { return USCAST( NOW - tInit ); }

#endif
