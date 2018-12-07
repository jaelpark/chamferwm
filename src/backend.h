#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>

namespace Compositor{
//declarations for friend classes
class X11ClientFrame;
class X11Background;
class X11Compositor;
class X11DebugCompositor;
}

namespace WManager{
class Client;
class Container;
}

namespace Backend{

class BackendProperty{
public:
	enum PROPERTY_TYPE{
		PROPERTY_TYPE_STRING,
		PROPERTY_TYPE_CLIENT,
		PROPERTY_TYPE_PIXMAP
	};
	BackendProperty(PROPERTY_TYPE);
	virtual ~BackendProperty();
	PROPERTY_TYPE type;
};

class BackendStringProperty : public BackendProperty{
public:
	BackendStringProperty(const char *);
	~BackendStringProperty();
	const char *pstr;
};

class BackendContainerProperty : public BackendProperty{
public:
	BackendContainerProperty(WManager::Container *);
	~BackendContainerProperty();
	WManager::Container *pcontainer;
};

class BackendPixmapProperty : public BackendProperty{
public:
	BackendPixmapProperty(xcb_pixmap_t);
	~BackendPixmapProperty();
	xcb_pixmap_t pixmap;
};

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
	//virtual void SetupEnvironment() = 0;
	virtual sint HandleEvent() = 0;
	virtual void MoveContainer(WManager::Container *, WManager::Container *) = 0;
	virtual const WManager::Container * GetRoot() const = 0;
	virtual const std::vector<std::pair<const WManager::Client *, WManager::Client *>> * GetStackAppendix() const = 0;
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
		const WManager::Rectangle *prect;
		const WManager::Client *pstackClient; //TODO: -> client
		const class X11Backend *pbackend;
		enum{
			CREATE_CONTAINED,
			CREATE_AUTOMATIC
		} mode;
		enum{
			HINT_DESKTOP = 0x1,
			HINT_ABOVE = 0x2,
			HINT_NO_INPUT = 0x4
		};
		uint hints;
		const BackendStringProperty *pwmName;
		const BackendStringProperty *pwmClass;
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
friend class Compositor::X11Background;
friend class Compositor::X11Compositor;
friend class Compositor::X11DebugCompositor;
public:
	X11Backend();
	virtual ~X11Backend();
	bool QueryExtension(const char *, sint *, sint *) const;
	xcb_atom_t GetAtom(const char *) const;
	void StackRecursiveAppendix(const WManager::Client *);
	void StackRecursive(const WManager::Container *);
	void StackClients();
	//void HandleTimer() const;
	enum MODE{
		MODE_UNDEFINED,
		MODE_MANUAL,
		MODE_AUTOMATIC
	};
	virtual X11Client * FindClient(xcb_window_t, MODE) const = 0;
	virtual void TimerEvent() = 0;
	//void * GetProperty(xcb_atom_t, xcb_atom_t) const;
	//void FreeProperty(...) const;
protected:
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
	xcb_window_t window; //root or test window
	xcb_timestamp_t lastTime;
	struct timespec eventTimer;
	struct timespec pollTimer;
	//bool polling;
	struct KeyBinding{
		xcb_keycode_t keycode;
		uint mask;
		uint keyId;
	};
	std::vector<KeyBinding> keycodes; //user defined id associated with the keycode
	xcb_ewmh_connection_t ewmh;
	xcb_window_t ewmh_window;
	
	std::deque<std::pair<const WManager::Client *, WManager::Client *>> appendixQueue;

	enum ATOM{
		//ATOM_CHAMFER_ALARM,
		ATOM_WM_PROTOCOLS,
		ATOM_WM_DELETE_WINDOW,
		ATOM_ESETROOT_PMAP_ID,
		ATOM_X_ROOTPMAP_ID,
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
	//void SetupEnvironment();
	sint HandleEvent();
	X11Client * FindClient(xcb_window_t, MODE) const;
protected:
	enum PROPERTY_ID{
		PROPERTY_ID_PIXMAP,
		PROPERTY_ID_WM_NAME,
		PROPERTY_ID_WM_CLASS,
		PROPERTY_ID_TRANSIENT_FOR,
	};
	virtual X11Client * SetupClient(const X11Client::CreateInfo *) = 0;
	virtual void PropertyChange(X11Client *, PROPERTY_ID, const BackendProperty *) = 0;
	virtual void DestroyClient(X11Client *) = 0;
private:
	xcb_keycode_t exitKeycode;
	std::vector<std::pair<X11Client *, MODE>> clients;
	std::vector<std::pair<xcb_window_t, WManager::Rectangle>> configCache;
	std::vector<X11Client *> unmappingQueue;
	std::vector<xcb_window_t> netClientList; //used only to update the property - not maintained
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
	//void SetupEnvironment();
	sint HandleEvent();
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

