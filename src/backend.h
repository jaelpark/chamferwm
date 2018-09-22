#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <vector> //client list

namespace Compositor{
//declarations for friend classes
class X11ClientFrame;
class X11Compositor;
class X11DebugCompositor;
}

namespace WManager{
class Client;
class Container;
}

namespace Backend{

class BackendEvent{
public:
	BackendEvent();
	virtual ~BackendEvent();
};

class BackendKeyBinder{
public:
	BackendKeyBinder();
	virtual ~BackendKeyBinder();
	virtual void BindKey(uint, uint, uint) = 0;
};

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void Start() = 0;
	//virtual sint GetEventFileDescriptor() = 0;
	virtual bool HandleEvent() = 0;
protected:
	//Functions defined by the implementing backends.
	virtual void DefineBindings(BackendKeyBinder *) = 0;
	virtual void EventNotify(const BackendEvent *) = 0;
	virtual void KeyPress(uint, bool) = 0;
};

class X11Event : public BackendEvent{
public:
	X11Event(xcb_generic_event_t *, const class X11Backend *);
	~X11Event();
	xcb_generic_event_t *pevent;
	const X11Backend *pbackend;
};

class X11KeyBinder : public BackendKeyBinder{
public:
	X11KeyBinder(xcb_key_symbols_t *, class X11Backend *);
	~X11KeyBinder();
	void BindKey(uint, uint, uint);
	xcb_key_symbols_t *psymbols;
	X11Backend *pbackend;
};

class X11Client : public WManager::Client{
public:
	struct CreateInfo{
		xcb_window_t window;
		const class X11Backend *pbackend;
	};
	//X11Client(xcb_window_t, const class X11Backend *);
	X11Client(WManager::Container *, const CreateInfo *);
	~X11Client();
	virtual void AdjustSurface1(){};
	void UpdateTranslation();
	//virtual void UpdateCompositor();
	xcb_window_t window;
	const X11Backend *pbackend;
};

class X11Backend : public BackendInterface{
friend class X11Event;
friend class X11KeyBinder;
friend class X11Client;
friend class Default;
friend class DebugClient;
friend class Compositor::X11ClientFrame;
friend class Compositor::X11Compositor;
friend class Compositor::X11DebugCompositor;
public:
	X11Backend();
	virtual ~X11Backend();
	bool QueryExtension(const char *, sint *, sint *) const;
	virtual X11Client * FindClient(xcb_window_t) const = 0;
protected:
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
	xcb_window_t window;
	struct KeyBinding{
		xcb_keycode_t keycode;
		uint mask;
		uint keyId;
	};
	std::vector<KeyBinding> keycodes; //user defined id associated with the keycode
};

class Default : public X11Backend{
public:
	Default();
	virtual ~Default();
	void Start();
	//sint GetEventFileDescriptor();
	bool HandleEvent();
	X11Client * FindClient(xcb_window_t) const;
protected:
	virtual X11Client * SetupClient(const X11Client::CreateInfo *) = 0;
	virtual void DestroyClient(X11Client *) = 0;
private:
	xcb_keycode_t exitKeycode;
	xcb_keycode_t launchKeycode;
	std::vector<X11Client *> clients;
};

class DebugClient : public WManager::Client{
public:
	struct CreateInfo{
		const class X11Backend *pbackend;
	};
	DebugClient(WManager::Container *, const CreateInfo *);
	~DebugClient();
	virtual void AdjustSurface1(){};
	void UpdateTranslation();
	const X11Backend *pbackend;
};

class Debug : public X11Backend{
public:
	Debug();
	virtual ~Debug();
	void Start();
	//sint GetEventFileDescriptor();
	bool HandleEvent();
	X11Client * FindClient(xcb_window_t) const;
protected:
	virtual DebugClient * SetupClient(const DebugClient::CreateInfo *) = 0;
	virtual void DestroyClient(DebugClient *) = 0;
private:
	xcb_keycode_t exitKeycode;
	xcb_keycode_t launchKeycode;
	xcb_keycode_t closeKeycode;
	std::vector<DebugClient *> clients;
};

}

#endif

