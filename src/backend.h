#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>
#include <vector> //client list

namespace Compositor{
//declarations for friend classes
class X11ClientFrame;
class X11Compositor;
class X11DebugCompositor;
}

namespace WManager{
class Client;
}

namespace Backend{

class BackendEvent{
public:
	BackendEvent();
	virtual ~BackendEvent();
};

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void Start() = 0;
	virtual sint GetEventFileDescriptor() = 0;
	virtual bool HandleEvent() = 0;
protected:
	//Functions defined by the implementing backends.
	virtual void DefineBindings() = 0;
	virtual void EventNotify(const BackendEvent *) = 0;
};

class X11Event : public BackendEvent{
public:
	X11Event(xcb_generic_event_t *);
	~X11Event();
	xcb_generic_event_t *pevent;
};

class X11Client : public WManager::Client{
public:
	struct CreateInfo{
		xcb_window_t window;
		const class X11Backend *pbackend;
	};
	X11Client(xcb_window_t, const class X11Backend *);
	X11Client(const struct CreateInfo *);
	~X11Client();
	WManager::Rectangle GetRect() const;
	xcb_window_t window;
	const X11Backend *pbackend;
};

class X11Backend : public BackendInterface{
friend class X11Client;
friend class Compositor::X11ClientFrame;
friend class Compositor::X11Compositor;
friend class Compositor::X11DebugCompositor;
public:
	X11Backend();
	virtual ~X11Backend();
	bool QueryExtension(const char *, sint *, sint *) const;
protected:
	virtual X11Client * SetupClient(const X11Client::CreateInfo *) = 0;
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
	std::vector<X11Client *> clients;
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

