#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>

//class xcb_connection_t;
//class xcb_screen_t;

namespace Compositor{
class Default;
}

namespace Backend{

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void Start() = 0;
	virtual sint GetEventFileDescriptor() = 0;
	virtual bool HandleEvent() = 0;
protected:
	//Functions called by the implementing backends.
	virtual void DefineBindings() = 0;
};

class X11Backend : public BackendInterface{
friend class Compositor::Default;
public:
	X11Backend();
	virtual ~X11Backend();
protected:
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
	xcb_window_t overlay;
};

class Default : public X11Backend{
friend class Compositor::Default;
public:
	Default();
	virtual ~Default();
	void Start();
	sint GetEventFileDescriptor();
	bool HandleEvent();
private:	
	xcb_keycode_t exitKeycode;
};

class Fake : public X11Backend{
friend class Compositor::Default;
public:
	Fake();
	virtual ~Fake();
	void Start();
	sint GetEventFileDescriptor();
	bool HandleEvent();
private:
	xcb_keycode_t exitKeycode;
};

}

#endif

