#ifndef CONFIG_H
#define CONFIG_H

#include "main.h"
#include <boost/python.hpp>

namespace Backend{
class BackendKeyBinder;
}

namespace Config{

class KeyConfigInterface{
public:
	KeyConfigInterface();
	~KeyConfigInterface();
	void SetKeyBinder(class Backend::BackendKeyBinder *);
	virtual void SetupKeys();
	Backend::BackendKeyBinder *pkeyBinder;

	static void Bind(boost::python::object);
	static KeyConfigInterface defaultInt;
	static KeyConfigInterface *pkeyConfigInt;
};

class KeyConfigProxy : public KeyConfigInterface, public boost::python::wrapper<KeyConfigInterface>{
public:
	KeyConfigProxy();
	~KeyConfigProxy();
	void BindKey(uint, uint, uint);
	void SetupKeys();
};

class KeyActionInterface{
public:
	KeyActionInterface();
	~KeyActionInterface();
	virtual void OnKeyPress(uint);
	virtual void OnKeyRelease(uint);
	static void Bind(boost::python::object);
	static KeyActionInterface defaultInt;
	static KeyActionInterface *pkeyActionInt;
};

class KeyActionProxy : public KeyActionInterface, public boost::python::wrapper<KeyActionInterface>{
public:
	KeyActionProxy();
	~KeyActionProxy();
	void OnKeyPress(uint);
	void OnKeyRelease(uint);
};

class CompositorInterface{
public:
	CompositorInterface();
	~CompositorInterface();
	std::string shaderPath;
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

