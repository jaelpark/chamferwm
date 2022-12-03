#ifndef BACKEND_H
#define BACKEND_H

#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_cursor.h>

namespace Compositor{
//declarations for friend classes
class X11ClientFrame;
class X11Background;
class X11Compositor;
class X11DebugCompositor;
class TexturePixmap;
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

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void Start() = 0;
	virtual void Stop() = 0;
	//virtual sint GetEventFileDescriptor() = 0;
	//virtual void SetupEnvironment() = 0;
	virtual sint HandleEvent(bool) = 0;
	virtual void MoveContainer(WManager::Container *, WManager::Container *) = 0;
	virtual void FloatContainer(WManager::Container *) = 0;
	virtual const std::vector<std::pair<const WManager::Client *, WManager::Client *>> * GetStackAppendix() const = 0;
	virtual void SortStackAppendix() = 0;
protected:
	//Functions defined by the implementing backends.
	virtual void DefineBindings() = 0;
	virtual bool EventNotify(const BackendEvent *) = 0;
	virtual void WakeNotify() = 0;
	virtual void KeyPress(uint, bool) = 0;
};

class X11Event : public BackendEvent{
public:
	X11Event(xcb_generic_event_t *, const class X11Backend *);
	~X11Event();
	xcb_generic_event_t *pevent;
	const X11Backend *pbackend;
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
			HINT_NO_INPUT = 0x4,
			HINT_FULLSCREEN = 0x8,
			HINT_FLOATING = 0x10
		};
		uint hints;
		const BackendStringProperty *pwmName;
		const BackendStringProperty *pwmClass;
	};
	X11Client(class X11Container *, const CreateInfo *);
	~X11Client();
	virtual void AdjustSurface1(){};
	virtual void Redirect1(){};
	virtual void StartComposition1(){};
	virtual void Unredirect1(){};
	virtual void StopComposition1(){};
	virtual void SetTitle1(const char *){};
	//virtual void SetFullscreen1(bool){};
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
	X11Container(class X11Backend *, bool);
	X11Container(WManager::Container *, const WManager::Container::Setup &, class X11Backend *);
	virtual ~X11Container();
	void Focus1();
	void Place1(WManager::Container *);
	void Stack1();
	void Fullscreen1();
	void SetClient(X11Client *);
	const X11Backend *pbackend;
	X11Client *pclient11;
	bool noComp;
};

class X11Backend : public BackendInterface{
friend class X11Event;
friend class X11Client;
friend class X11Container;
friend class DebugClient;
friend class Compositor::X11ClientFrame;
friend class Compositor::X11Background;
friend class Compositor::X11Compositor;
friend class Compositor::X11DebugCompositor;
friend class Compositor::TexturePixmap;
public:
	X11Backend(bool);
	virtual ~X11Backend();
	bool QueryExtension(const char *, sint *, sint *) const;
	xcb_atom_t GetAtom(const char *) const;
	void StackRecursiveAppendix(const WManager::Client *, const WManager::Container *);
	void StackRecursive(const WManager::Container *, const WManager::Container *);
	void StackClients(const WManager::Container *);
	void ForEachRecursive(X11Container *, WManager::Container *, void (*)(X11Container *, WManager::Client *));
	void ForEach(X11Container *, void (*)(X11Container *, WManager::Client *));
	void BindKey(uint, uint, uint);
	void MapKey(uint, uint, uint);
	void GrabKeyboard(bool);
	//void HandleTimer() const;
	enum MODE{
		MODE_UNDEFINED,
		MODE_MANUAL,
		MODE_AUTOMATIC
	};
	virtual X11Client * FindClient(xcb_window_t, MODE) const = 0;
	virtual void TimerEvent() = 0;
	virtual bool ApproveExternal(const BackendStringProperty *, const BackendStringProperty *) = 0;
	//void * GetProperty(xcb_atom_t, xcb_atom_t) const;
	//void FreeProperty(...) const;
protected:
	xcb_connection_t *pcon;
	xcb_screen_t *pscr;
	xcb_key_symbols_t *psymbols;
	xcb_window_t window; //root or test window
	xcb_timestamp_t lastTime;
	struct timespec eventTimer;
	struct timespec pollTimer;
	struct timespec inputTimer; //track time since last keypress
	//bool polling;
	struct KeyBinding{
		xcb_keycode_t keycode;
		uint mask;
		uint keyId;
	};
	std::vector<KeyBinding> keycodes; //user defined id associated with the keycode
	xcb_ewmh_connection_t ewmh;
	xcb_window_t ewmh_window;
	
public:
	std::deque<WManager::Client *> clientStack;
	WManager::Client *pfocusInClient; //from XCB_FOCUS_IN events (uncontained clients)
protected:
	std::deque<std::pair<const WManager::Client *, WManager::Client *>> appendixQueue; //no need to store, rather keep it here to avoid repeated construction
	bool standaloneComp;

	enum ATOM{
		//ATOM_CHAMFER_ALARM,
		//add to patomStrs
		ATOM_WM_PROTOCOLS,
		ATOM_WM_DELETE_WINDOW,
		ATOM_WM_TAKE_FOCUS,
		ATOM_ESETROOT_PMAP_ID,
		ATOM_X_ROOTPMAP_ID,
		ATOM_COUNT
	};
	xcb_atom_t atoms[ATOM_COUNT];
	static const char *patomStrs[ATOM_COUNT];

	xcb_cursor_context_t *pcursorctx;
	enum CURSOR{
		CURSOR_POINTER,
		CURSOR_COUNT
	};
	xcb_cursor_t cursors[CURSOR_COUNT];
	static const char *pcursorStrs[CURSOR_COUNT];
};

class Default : public X11Backend{
public:
	Default(bool);
	virtual ~Default();
	void Start();
	void Stop();
	sint HandleEvent(bool);
	X11Client * FindClient(xcb_window_t, MODE) const;
protected:
	enum PROPERTY_ID{
		PROPERTY_ID_PIXMAP,
		PROPERTY_ID_WM_NAME,
		PROPERTY_ID_WM_CLASS,
		PROPERTY_ID_TRANSIENT_FOR,
	};
	virtual X11Client * SetupClient(const X11Client::CreateInfo *) = 0;
	virtual WManager::Container * CreateWorkspace(const char *) = 0;
	virtual void SetFullscreen(X11Client *, bool) = 0;
	virtual void SetFocus(X11Client *) = 0;
	virtual void Enter(X11Client *) = 0;
	virtual void PropertyChange(X11Client *, PROPERTY_ID, const BackendProperty *) = 0;
	virtual void DestroyClient(X11Client *) = 0;
private:
	xcb_keycode_t exitKeycode;
	//xcb_keycode_t testKeycode;
	typedef std::tuple<xcb_window_t, WManager::Rectangle, xcb_window_t> ConfigCacheElement;
	std::vector<std::pair<X11Client *, MODE>> clients;
	std::vector<ConfigCacheElement> configCache;
	std::vector<xcb_window_t> netClientList; //used only to update the property - not maintained
	std::set<X11Client *> unmappingQueue;
	X11Client *pdragClient;
	sint dragRootX, dragRootY;
};

class DebugClient : public WManager::Client{
public:
	struct CreateInfo{
		const class X11Backend *pbackend;
	};
	DebugClient(class DebugContainer *, const CreateInfo *);
	~DebugClient();
	virtual void AdjustSurface1(){};
	virtual void SetTitle1(const char *){};
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
	//void Fullscreen1();
	void SetClient(DebugClient *);
	const X11Backend *pbackend;
	DebugClient *pdebugClient;
};

class Debug : public X11Backend{
public:
	Debug();
	virtual ~Debug();
	void Start();
	void Stop();
	sint HandleEvent(bool);
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

