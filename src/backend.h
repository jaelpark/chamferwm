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
	X11Client(const CreateInfo *);
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
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
};

class Default : public X11Backend{
public:
	Default();
	virtual ~Default();
	void Start();
	sint GetEventFileDescriptor();
	bool HandleEvent();
protected:
	virtual X11Client * SetupClient(const X11Client::CreateInfo *) = 0;
private:	
	xcb_keycode_t exitKeycode;
	std::vector<X11Client *> clients;
};

class DebugClient : public WManager::Client{
public:
	struct CreateInfo{
		WManager::Rectangle rect;
		const class Debug *pbackend;
	};
	DebugClient(WManager::Rectangle);
	DebugClient(const CreateInfo *);
	~DebugClient();
	WManager::Rectangle GetRect() const;
	sint x, y, w, h;
};

class Debug : public X11Backend{
public:
	Debug();
	virtual ~Debug();
	void Start();
	sint GetEventFileDescriptor();
	bool HandleEvent();
protected:
	virtual DebugClient * SetupClient(const DebugClient::CreateInfo *) = 0;
private:
	xcb_keycode_t exitKeycode;
	xcb_keycode_t spaceKeycode;
	std::vector<DebugClient *> clients;
};

}

#endif

