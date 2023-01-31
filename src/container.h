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
	Rectangle titleRect;
	glm::vec2 position; //interpolated, animated rectangle
	glm::vec2 titlePad1; //title pad in pixels
	glm::vec2 titleStackOffset;
	glm::vec2 titleFrameExtent;
	uint stackIndex;
	struct timespec translationTime;
	class Container *pcontainer;
};

class Container{
public:
	enum FLAG{
		FLAG_FLOATING = 0x1,
		FLAG_NO_FOCUS = 0x2,
		FLAG_FULLSCREEN = 0x4,
		FLAG_STACKED = 0x8
	};
	enum LAYOUT{
		LAYOUT_VSPLIT,
		LAYOUT_HSPLIT,
	};
	enum TITLEBAR{
		TITLEBAR_NONE,
		TITLEBAR_LEFT,
		TITLEBAR_TOP,
		TITLEBAR_RIGHT,
		TITLEBAR_BOTTOM
		//TITLEBAR_AUTO_TOP_LEFT,
		//TITLEBAR_AUTO_BOTTOM_RIGHT,
	};
	struct Setup{
		const char *pname = 0;
		glm::vec2 canvasOffset = glm::vec2(0.0f);
		glm::vec2 canvasExtent = glm::vec2(0.0f);
		//Border width can be set anytime before the client creation
		glm::vec2 margin = glm::vec2(0.0f);
		//For performance reasons, the min/maxSize has to be known before the container is created.
		glm::vec2 size = glm::vec2(1.0f);
		glm::vec2 minSize = glm::vec2(0.0f);
		glm::vec2 maxSize = glm::vec2(1.0f);
		uint flags = 0;
		TITLEBAR titleBar = TITLEBAR_NONE;
		bool titleStackOnly = false;
		float titlePad = 0.0f;
	};
	Container(); //root container
	Container(Container *, const Setup &);
	virtual ~Container();
	void AppendRoot(Container *);
	void Place(Container *);
	Container * Remove();
	Container * Collapse();
	void Focus();
	void SetFullscreen(bool);
	void SetStacked(bool);
	//void SetFloating(bool);
	Container * GetNext();
	Container * GetPrev();
	Container * GetParent() const;
	Container * GetFocus() const;
	Container * GetRoot();
	void SetName(const char *);
	void SetSize(glm::vec2);
	void SetTitlebar(TITLEBAR, bool = true);

	void MoveNext();
	void MovePrev();

	glm::vec2 GetMinSize() const;
	void TranslateRecursive(glm::vec2, glm::vec2, glm::vec2, glm::vec2);
	void Translate();
	void StackRecursive();
	void Stack();
	void SetLayout(LAYOUT);

	virtual void Focus1(){}
	virtual void Place1(WManager::Container *){}
	virtual void Stack1(){}
	virtual void Fullscreen1(){}

	Container *pParent;
	Container *pch; //First children
	Container *pnext; //Subsequent container in the parent container
	Container *pRootNext; //Parallel workspace. To avoid bugs, pnext (null for root containers) will not be used for this.
	std::deque<Container *> focusQueue;
	std::deque<Container *> stackQueue;

	Client *pclient;
	char *pname; //name identifier for searches

	//absolute normalized coordinates
	glm::vec2 p;
	glm::vec2 posFullCanvas;
	glm::vec2 e;
	glm::vec2 extFullCanvas;
	glm::vec2 canvasOffset; //should be multiplied by e?
	glm::vec2 canvasExtent;

	glm::vec2 margin;
	glm::vec2 titlePad; //title size either horizontally or vertically - depends on font size
	glm::vec2 titleSpan; //title span in normalized coords: region that the title bar spans
	glm::mat2x2 titleTransform;
	float absTitlePad;

	glm::vec2 size; //size relative to the parent container, V split and H split size. When floating, this is the actual size (rect).
	glm::vec2 minSize; //min size, relative to screen
	glm::vec2 maxSize; //max size, relative to screen

	uint flags;
	LAYOUT layout;
	TITLEBAR titleBar;
	bool titleStackOnly; //title is rendered only when stacking

	static WManager::Container *ptreeFocus; //client focus, managed by Python
	static std::deque<std::pair<WManager::Container *, struct timespec>> tiledFocusQueue;
	static std::deque<WManager::Container *> floatFocusQueue;
	static std::deque<WManager::Container *> rootQueue; //Root container queue in the order of creation. Mainly used for EWMH
};

//for diamond inheritance:
/*class RootContainer : Container{
public:
	RootContainer(const char *);
	virtual ~RootContainer();
	void Link(RootContainer *);
	RootContainer *pRootNext; //Parallel workspace. To avoid bugs, pnext (null for root containers) will not be used for this.
	char *pname;
	//noComp
};*/

}

#endif

