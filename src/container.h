#ifndef CONTAINER_H
#define CONTAINER_H

namespace WManager{

struct Rectangle{
	sint x;
	sint y;
	sint w;
	sint h;
};

class Client{
public:
	//Client(class BackendInterface *);
	Client(class Container *);
	virtual ~Client();
	//
	//virtual functions for the backend implementation
	virtual void UpdateTranslation(){}// = 0;
	virtual void Kill(){}// = 0;
	Rectangle rect; //pixel coordinates of the client
	Rectangle oldRect; //to animate the transitions
	glm::vec2 position; //interpolated, animated rectangle
	struct timespec translationTime;
	class Container *pcontainer;
};

class Container{
public:
	enum FLAG{
		FLAG_FLOATING = 0x1,
		FLAG_NO_FOCUS = 0x2,
		FLAG_FULLSCREEN = 0x4
	};
	struct Setup{
		glm::vec2 canvasOffset = glm::vec2(0.0f);
		glm::vec2 canvasExtent = glm::vec2(0.0f);
		//Border width can be set anytime before the client creation
		glm::vec2 borderWidth = glm::vec2(0.0f);
		//For performance reasons, the min/maxSize has to be known before the container is created.
		glm::vec2 minSize = glm::vec2(0.0f);
		glm::vec2 maxSize = glm::vec2(1.0f);
		uint flags = 0;
		/*enum INSERT{
			INSERT_APPEND,
			INSERT_REPARENT //always with client
		} insert = INSERT_APPEND;*/
	};
	Container(); //root container
	Container(Container *, const Setup &);
	virtual ~Container();
	void Place(Container *);
	Container * Remove();
	Container * Collapse();
	void Focus();
	void SetFullscreen(bool);
	Container * GetNext();
	Container * GetPrev();
	Container * GetParent() const;
	Container * GetFocus() const;
	Container * GetRoot();

	void MoveNext();
	void MovePrev();

	glm::vec2 GetMinSize() const;
	void TranslateRecursive(glm::vec2, glm::vec2, glm::vec2, glm::vec2);
	void Translate();
	void StackRecursive();
	void Stack();
	enum LAYOUT{
		LAYOUT_VSPLIT,
		LAYOUT_HSPLIT,
	};
	void SetLayout(LAYOUT);

	virtual void Focus1(){}// = 0;
	virtual void Stack1(){}// = 0;
	virtual void Fullscreen1(){}

	Container *pParent;
	Container *pch; //First children
	Container *pnext; //Subsequent container in the parent container
	std::deque<Container *> focusQueue;
	std::deque<Container *> stackQueue;

	Client *pclient;

	//glm::vec2 scale;
	// = 1.0f: default size, scale with the number of containers
	// < 1.0f: smaller than default, leaving the unscaled parallel containers more space
	//glm::vec2 adjustScale;

	//absolute normalized coordinates
	glm::vec2 p;
	glm::vec2 posFullCanvas;
	glm::vec2 e;
	glm::vec2 extFullCanvas;
	glm::vec2 canvasOffset; //should be multiplied by e?
	glm::vec2 canvasExtent;

	//glm::vec2 c1; //iteration center
	glm::vec2 e1; //iteration e

	glm::vec2 borderWidth;

	glm::vec2 minSize;
	glm::vec2 maxSize;

	uint flags;
	LAYOUT layout;

	static WManager::Container *ptreeFocus; //client focus, managed by Python
	static std::deque<std::pair<WManager::Container *, struct timespec>> tiledFocusQueue;
	static std::deque<WManager::Container *> floatFocusQueue;
};

}

#endif

