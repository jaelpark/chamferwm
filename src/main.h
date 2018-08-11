#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef unsigned int uint;
typedef int sint;

void DebugPrintf(FILE *, const char *, ...);

#define mstrdup(s) strcpy(new char[strlen(s+1)],s)
#define mstrfree(s) delete []s

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

class Clock{
public:
	Clock();
	~Clock();
	void Step();
	float GetTimeDelta() const;
private:
	struct timespec step1, step0;
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
