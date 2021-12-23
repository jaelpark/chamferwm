#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <set>
#include <tuple>

typedef unsigned int uint;
typedef int sint;

typedef unsigned long long int uint64;
typedef long long int sint64;

void DebugPrintf(FILE *, const char *, ...);

#define mstrdup(s) strcpy(new char[strlen(s)+1],s)
#define mstrndup(s,n) strncpy(new char[n+1],s,n)
#define mstrfree(s) delete []s

#define timespec_diff(b,a) (double)(b.tv_sec-a.tv_sec)+(double)((b.tv_nsec-a.tv_nsec)/1e9)
#define timespec_diff_sec(b,a) (b.tv_sec-a.tv_sec)

static inline void timespec_diff_ptr(struct timespec &b, struct timespec &a, struct timespec &r){
	if(b.tv_nsec < a.tv_nsec){
		r.tv_sec = b.tv_sec-1-a.tv_sec;
		r.tv_nsec = b.tv_nsec-a.tv_nsec+1e9;
	}else{
		r.tv_sec = b.tv_sec-a.tv_sec;
		r.tv_nsec = b.tv_nsec-a.tv_nsec;
	}
}

template<class T>
static inline bool any(T t, T *parray, uint n){
	for(uint i = 0; i < n; ++i)
		if(t == parray[i])
			return true;
	return false;
}

class Exception{
public:
	Exception();
	Exception(const char *);
	~Exception();
	const char * what();
	static char buffer[4096];
private:
	const char *pmsg;
};

class Blob{
public:
	Blob(const char *);
	~Blob();
	const char * GetBufferPointer() const;
	size_t GetBufferLength() const;
private:
	char *pbuffer;
	size_t buflen;
};

#endif
