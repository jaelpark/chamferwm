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
	virtual bool OnFocus();
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
	//float titlePadding;
	//client variables
	uint shaderUserFlags;
	//--------------------------------
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
	bool OnFocus();
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
	X11ContainerConfig(class Backend::X11Backend *);
	~X11ContainerConfig();
};

class DebugContainerConfig : public Backend::DebugContainer, public ContainerConfig{
public:
	DebugContainerConfig(ContainerInterface *, WManager::Container *, const WManager::Container::Setup &, class Backend::X11Backend *);
	DebugContainerConfig(class Backend::X11Backend *);
	~DebugContainerConfig();
};

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void OnSetupKeys(bool);
	virtual boost::python::object OnCreateContainer();
	virtual void OnKeyPress(uint);
	virtual void OnKeyRelease(uint);
	virtual void OnTimer();
	virtual void OnExit();
	boost::python::object GetFocus();
	boost::python::object GetRoot();
	void BindKey(uint, uint, uint);
	void MapKey(uint, uint, uint);
	void GrabKeyboard(bool);

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
	~CompositorInterface();

	sint deviceIndex;
	bool debugLayers;
	bool scissoring;
	bool hostMemoryImport;
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
	void Run(const char *, const char *);

	static sint deviceIndex;
	static bool debugLayers;
	static bool scissoring;
	static bool hostMemoryImport;
};

}

#endif

