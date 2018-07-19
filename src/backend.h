#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>

//class xcb_connection_t;
//class xcb_screen_t;

namespace Compositor{
class X11Compositor;
}

namespace WManager{
class Client;
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
	//
	//std::vector<Client *> clients;
};

class X11Client : public WManager::Client{
public:
	X11Client(xcb_window_t, const class X11Backend *);
	~X11Client();
	WManager::Rectangle GetRect() const;
	xcb_window_t window;
	xcb_pixmap_t windowPixmap;
	const X11Backend *pbackend;
};

class X11Backend : public BackendInterface{
friend class X11Client;
friend class Compositor::X11Compositor;
public:
	X11Backend();
	virtual ~X11Backend();
protected:
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
	xcb_window_t overlay;
};

class Default : public X11Backend{
public:
	Default();
	virtual ~Default();
	void Start();
	sint GetEventFileDescriptor();
	bool HandleEvent();
private:	
	xcb_keycode_t exitKeycode;
};

class FakeClient : public WManager::Client{
public:
	FakeClient(sint, sint, sint, sint);
	~FakeClient();
	WManager::Rectangle GetRect() const;
	sint x, y, w, h;
};

class Fake : public X11Backend{
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

