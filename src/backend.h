#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>

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
	virtual WManager::Container * GetRoot() const = 0;
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
		const WManager::Rectangle *prect;
		const WManager::Container *pstackContainer;
		const class X11Backend *pbackend;
	};
	X11Client(WManager::Container *, const CreateInfo *);
	~X11Client();
	virtual void AdjustSurface1(){};
	void UpdateTranslation(); //manual mode update
	void UpdateTranslation(const WManager::Rectangle *); //automatic mode update
	bool ProtocolSupport(xcb_atom_t);
	void Kill();
	xcb_window_t window;
	enum FLAG{
		FLAG_UNMAPPING = 0x1
	};
	uint flags;
	const X11Backend *pbackend;
};

class X11Container : public WManager::Container{
public:
	X11Container(class X11Backend *);
	X11Container(WManager::Container *, const WManager::Container::Setup &, class X11Backend *);
	virtual ~X11Container();
	void Focus1();
	void StackRecursive(WManager::Container *);
	void Stack1();
	const X11Backend *pbackend;
};

class X11Backend : public BackendInterface{
friend class X11Event;
friend class X11KeyBinder;
friend class X11Client;
friend class X11Container;
friend class DebugClient;
friend class Compositor::X11ClientFrame;
friend class Compositor::X11Compositor;
friend class Compositor::X11DebugCompositor;
public:
	X11Backend();
	virtual ~X11Backend();
	bool QueryExtension(const char *, sint *, sint *) const;
	xcb_atom_t GetAtom(const char *) const;
	enum MODE{
		MODE_UNDEFINED,
		MODE_MANUAL,
		MODE_AUTOMATIC
	};
	virtual X11Client * FindClient(xcb_window_t, MODE) const = 0;
	//void * GetProperty(xcb_atom_t, xcb_atom_t) const;
	//void FreeProperty(...) const;
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
	xcb_ewmh_connection_t ewmh;

	enum ATOM{
		ATOM_WM_PROTOCOLS,
		ATOM_WM_DELETE_WINDOW,
		ATOM_COUNT
	};
	xcb_atom_t atoms[ATOM_COUNT];
	static const char *patomStrs[ATOM_COUNT];
};

class Default : public X11Backend{
public:
	Default();
	virtual ~Default();
	void Start();
	//sint GetEventFileDescriptor();
	bool HandleEvent();
	X11Client * FindClient(xcb_window_t, MODE) const;
protected:
	virtual X11Client * SetupClient(const X11Client::CreateInfo *) = 0;
	virtual void DestroyClient(X11Client *) = 0;
private:
	xcb_keycode_t exitKeycode;
	std::vector<std::pair<X11Client *, MODE>> clients;
	std::vector<std::pair<xcb_window_t, WManager::Rectangle>> configCache;
	std::vector<X11Client *> unmappingQueue;
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
	void Kill();
	const X11Backend *pbackend;
};

class DebugContainer : public WManager::Container{
public:
	DebugContainer(class X11Backend *);
	DebugContainer(WManager::Container *, const WManager::Container::Setup &, class X11Backend *);
	virtual ~DebugContainer();
	void Focus1();
	void Stack1();
	const X11Backend *pbackend;
};

class Debug : public X11Backend{
public:
	Debug();
	virtual ~Debug();
	void Start();
	//sint GetEventFileDescriptor();
	bool HandleEvent();
	X11Client * FindClient(xcb_window_t, MODE) const;
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

