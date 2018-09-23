#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <glm/glm.hpp>

#include <vector>
#include <deque>

typedef unsigned int uint;
typedef int sint;

typedef unsigned long long int uint64;
typedef long long int sint64;

void DebugPrintf(FILE *, const char *, ...);

#define mstrdup(s) strcpy(new char[strlen(s+1)],s)
#define mstrfree(s) delete []s

#define timespec_diff(b,a) (float)(b.tv_sec-a.tv_sec)+(float)((b.tv_nsec-a.tv_nsec)/1e9)

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

/*class Clock{
public:
	Clock();
	~Clock();
	void Step();
	float GetTimeDelta() const;
private:
	struct timespec step1, step0;
};*/

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
