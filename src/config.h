#ifndef CONFIG_H
#define CONFIG_H

#include <boost/python.hpp>

namespace Backend{
class BackendKeyBinder;
class X11KeyBinder;
}

namespace WManager{
class Client;
class Container;
}

namespace Config{

class ContainerInterface{
public:
	ContainerInterface();
	virtual ~ContainerInterface();
	virtual void OnSetup();
	//void Link(container); //copy the settings to the container
	boost::python::tuple borderWidth;
	boost::python::tuple minSize;
	boost::python::tuple maxSize;
};

class ContainerProxy : public ContainerInterface, public boost::python::wrapper<ContainerInterface>{
public:
	ContainerProxy();
	~ContainerProxy();
	void OnSetup();
};

class X11ContainerConfig : public Backend::X11Container{
public:
	X11ContainerConfig(ContainerInterface *, WManager::Container *, class Backend::X11Backend *);
	//X11ContainerConfig(class Backend::X11Backend *);
	~X11ContainerConfig();
	ContainerInterface *pcontainerInt;
};

class DebugContainerConfig : public Backend::DebugContainer{
public:
	DebugContainerConfig(ContainerInterface *, WManager::Container *, class Backend::X11Backend *);
	DebugContainerConfig(class Backend::X11Backend *);
	~DebugContainerConfig();
	ContainerInterface *pcontainerInt;
};

class ClientProxy{
public:
	ClientProxy();
	ClientProxy(WManager::Client *);
	~ClientProxy();
	WManager::Container * GetContainer() const;
	WManager::Client *pclient;
};

class BackendInterface{
public:
	BackendInterface();
	virtual ~BackendInterface();
	virtual void OnSetupKeys(Backend::X11KeyBinder *);
	//virtual void OnSetupClient(const SetupProxy &);
	virtual boost::python::object OnCreateContainer();
	virtual void OnCreateClient(const ClientProxy &);
	virtual void OnKeyPress(uint);
	virtual void OnKeyRelease(uint);

	static void Bind(boost::python::object);
	static void SetFocus(WManager::Container *);
	static WManager::Container * GetFocus();

	static BackendInterface defaultInt;
	static BackendInterface *pbackendInt;
	static WManager::Container *pfocus; //client focus, managed by Python
};

class BackendProxy : public BackendInterface, public boost::python::wrapper<BackendInterface>{
public:
	BackendProxy();
	~BackendProxy();
	//
	void OnSetupKeys(Backend::X11KeyBinder *);
	//void OnSetupClient(const SetupProxy &);
	boost::python::object OnCreateContainer();
	void OnCreateClient(const ClientProxy &);
	void OnKeyPress(uint);
	void OnKeyRelease(uint);
};

class CompositorInterface{
public:
	CompositorInterface();
	~CompositorInterface();
	std::string shaderPath;
	//virtual void SetupShaders();

	static void Bind(boost::python::object);
	static CompositorInterface defaultInt;
	static CompositorInterface *pcompositorInt;
};

class CompositorProxy : public CompositorInterface, public boost::python::wrapper<CompositorInterface>{
public:
	CompositorProxy();
	~CompositorProxy();
};

class Loader{
public:
	Loader(const char *);
	~Loader();
	void Run(const char *, const char *);
};

}

#endif

