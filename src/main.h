#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <string.h>

typedef unsigned int uint;
typedef int sint;

void DebugPrintf(FILE *, const char *, ...);

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
	const char * GetBufferPointer();
	size_t GetBufferLength();
private:
	char *pbuffer;
	size_t buflen;
};

#endif
