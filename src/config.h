#ifndef CONFIG_H
#define CONFIG_H

#include <boost/python.hpp>

namespace WManager{
class Client;
class Container;
}

namespace Config{

class ContainerInterface{
public:
	ContainerInterface();
	virtual ~ContainerInterface();
	void CopySettingsSetup(WManager::Container::Setup &);
	void DeferredPropertyTransfer();
	virtual void OnSetupContainer();
	virtual void OnSetupClient();
	virtual boost::python::object OnParent();
	virtual void OnCreate();
	enum PROPERTY_ID{
		PROPERTY_ID_NAME,
		PROPERTY_ID_CLASS
	};
	virtual bool OnFullscreen(bool);
	virtual void OnStack(bool);
	virtual void OnFloat(bool);
	virtual bool OnFocus();
	virtual void OnEnter();
	virtual void OnPropertyChange(PROPERTY_ID);
	boost::python::object GetNext() const;
	boost::python::object GetPrev() const;
	boost::python::object GetParent() const;
	boost::python::object GetFocus() const;
	boost::python::object GetTiledFocus() const;
	boost::python::object GetFloatFocus() const;
	void Move(boost::python::object);
//public:
	//temporary storage for deferred assignment (before container is created)
	boost::python::tuple canvasOffset;
	boost::python::tuple canvasExtent;
	boost::python::tuple margin;
	boost::python::tuple size;
	boost::python::tuple minSize;
	boost::python::tuple maxSize;
	enum FLOAT{
		FLOAT_AUTOMATIC,
		FLOAT_ALWAYS,
		FLOAT_NEVER
	} floatingMode;
	WManager::Container::TITLEBAR titleBar;
	bool titleStackOnly;
	//float titlePadding;
	//client variables
	uint shaderUserFlags;
	//--------------------------------
	std::string name;
	std::string vertexShader;
	std::string geometryShader;
	std::string fragmentShader;
	std::string wm_name;
	std::string wm_class;

	WManager::Container *pcontainer;
	boost::python::object self;
	
	static void UpdateShaders();
	static std::set<ContainerInterface *> shaderUpdateQueue;
};

class ContainerProxy : public ContainerInterface, public boost::python::wrapper<ContainerInterface>{
public:
	ContainerProxy();
	~ContainerProxy();
	void OnSetupContainer();
	void OnSetupClient();
	boost::python::object OnParent();
	void OnCreate();
	bool OnFullscreen(bool);
	void OnStack(bool);
	void OnFloat(bool);
	bool OnFocus();
	void OnEnter();
	void OnPropertyChange(PROPERTY_ID);
};

class ContainerConfig{
public:
	ContainerConfig(ContainerInterface *, class Backend::X11Backend *);
	ContainerConfig(class Backend::X11Backend *);
	virtual ~ContainerConfig();
	ContainerInterface *pcontainerInt;
	class Backend::X11Backend *pbackend;
};

/*template<typename T>
class BackendContainerConfig : public T, public ContainerConfig{
public:
	BackendContainerConfig(ContainerInterface *, WManager::Container *, const WManager::Container::Setup &, class Backend::X11Backend *);
	BackendContainerConfig(class Backend::X11Backend *);
	~BackendContainerConfig();
};*/

class X11ContainerConfig : public Backend::X11Container, public ContainerConfig{
public:
	X11ContainerConfig(ContainerInterface *, WManager::Container *, const WManager::Container::Setup &, class Backend::X11Backend *);
	X11ContainerConfig(class Backend::X11Backend *, bool = false);
	~X11ContainerConfig();
};

class DebugContainerConfig : public Backend::DebugContainer, public ContainerConfig{
public:
	DebugContainerConfig(ContainerInterface *, WManager::Container *, const WManager::Container::Setup &, class Backend::X11Backend *);
	DebugContainerConfig(class Backend::X11Backend *);
	~DebugContainerConfig();
};

/*class WorkspaceInterface{
public:
	WorkspaceInterface();
	virtual ~WorkspaceInterface();
};

class WorkspaceProxy : public WorkspaceInterface, public boost::python::wrapper<WorkspaceInterface>{
public:
	WorkspaceProxy();
	~WorkspaceProxy();
};*/

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void OnSetupKeys(bool);
	virtual boost::python::object OnCreateContainer();
	//virtual boost::python::object OnCreateWorkspace();
	virtual void OnKeyPress(uint);
	virtual void OnKeyRelease(uint);
	virtual void OnTimer();
	virtual void OnExit();
	boost::python::object GetFocus();
	boost::python::object GetRoot(boost::python::object = boost::python::object());
	void BindKey(uint, uint, uint);
	void MapKey(uint, uint, uint);
	void GrabKeyboard(bool);

	bool standaloneComp;

	class BackendConfig *pbackend;

	static void Bind(boost::python::object);

	static BackendInterface defaultInt;
	static BackendInterface *pbackendInt;
};

class BackendProxy : public BackendInterface, public boost::python::wrapper<BackendInterface>{
public:
	BackendProxy();
	~BackendProxy();
	//
	void OnSetupKeys(bool);
	boost::python::object OnCreateContainer();
	boost::python::object OnCreateWorkspace();
	void OnKeyPress(uint);
	void OnKeyRelease(uint);
	void OnTimer();
	void OnExit();
};

class BackendConfig{
public:
	BackendConfig(BackendInterface *);
	virtual ~BackendConfig();
	BackendInterface *pbackendInt;
};

class CompositorInterface{
public:
	CompositorInterface();
	virtual ~CompositorInterface();
	virtual bool OnRedirectExternal(std::string, std::string);

	bool noCompositor;
	sint deviceIndex;
	bool debugLayers;
	bool scissoring;
	bool incrementalPresent;
	//bool hostMemoryImport;
	Compositor::CompositorInterface::IMPORT_MODE memoryImportMode;
	bool unredirOnFullscreen;
	bool enableAnimation;
	float animationDuration;

	std::string fontName;
	uint fontSize;

	class CompositorConfig *pcompositor;

	static void Bind(boost::python::object);

	static CompositorInterface defaultInt;
	static CompositorInterface *pcompositorInt;
};

class CompositorProxy : public CompositorInterface, public boost::python::wrapper<CompositorInterface>{
public:
	CompositorProxy();
	~CompositorProxy();
	bool OnRedirectExternal(std::string, std::string);
};

class CompositorConfig{
public:
	CompositorConfig(CompositorInterface *);
	virtual ~CompositorConfig();
	CompositorInterface *pcompositorInt;
};

class Loader{
public:
	Loader(const char *);
	~Loader();
	bool Run(const char *, const char *);

	//backend
	static bool standaloneComp;

	//compositor
	static bool noCompositor;
	static sint deviceIndex;
	static bool debugLayers;
	static bool scissoring;
	static bool incrementalPresent;
	//static bool hostMemoryImport;
	static Compositor::CompositorInterface::IMPORT_MODE memoryImportMode;
	static bool unredirOnFullscreen;
};

}

#endif

