#ifndef CONTAINER_H
#define CONTAINER_H

namespace WManager{

struct Rectangle{
	sint x;
	sint y;
	uint w;
	uint h;
};

class Client{
public:
	//Client(class BackendInterface *);
	Client(class Container *);
	virtual ~Client();
	//
	//virtual functions for the backend implementation
	virtual void UpdateTranslation() = 0;
	//Resize
	//SetLayer
	//virtual Rectangle GetRect() const = 0;
	Rectangle rect; //pixel coordinates of the client
	//TODO: scale; //scaling of the client inside the container
	class Container *pcontainer;
};

class Container{
	//TODO: MODE_APPEND and MODE_REPLACE_REPARENT. Or just directly determine when to reparent (the parent has a client)
public:
	struct Setup{
		//Container *preplace = 0; //temp: move to constructor params
		//Border width can be set anytime before the client creation
		glm::vec2 borderWidth = glm::vec2(0.0f);
		//For performance reasons, the min/maxSize has to be known before the container is created.
		glm::vec2 minSize = glm::vec2(0.0f);
		glm::vec2 maxSize = glm::vec2(1.0f);
	};
	Container(); //root container
	Container(Container *, const Setup &);
	virtual ~Container();
	Container * Remove();
	Container * Collapse();
	void Focus();
	Container * GetNext();
	Container * GetPrev();
	Container * GetParent();
	Container * GetFocus();
	enum ADJACENT{
		ADJACENT_LEFT,
		ADJACENT_RIGHT,
		ADJACENT_UP,
		ADJACENT_DOWN
	};
	Container * GetAdjacent(ADJACENT);

	void MoveNext();
	void MovePrev();

	glm::vec2 GetMinSize() const;
	void TranslateRecursive(glm::vec2, glm::vec2);
	void Translate();
	void Stack();
	//Stack order: pnext is always below the current one
	enum LAYOUT{
		LAYOUT_VSPLIT,
		LAYOUT_HSPLIT,
		//Title is rendered as tabs on the same row. Render titles inside a chamfered box.
		//stack mode: render titles vertically to the left?
		//LAYOUT_STACKED, //Do we need these modes? Stacking is already implemented with min window sizes.
		//LAYOUT_TABBED,
	};
	void SetLayout(LAYOUT);

	virtual void Focus1(){}// = 0;
	virtual void Stack1(){}// = 0;

	Container *pParent;
	Container *pch; //First children
	Container *pnext; //Subsequent container in the parent container
	std::deque<Container *> focusQueue;
	std::deque<Container *> stackQueue;

	Client *pclient;

	glm::vec2 scale;
	// = 1.0f: default size, scale with the number of containers
	// < 1.0f: smaller than default, leaving the unscaled parallel containers more space
	//glm::vec2 adjustScale;

	//absolute normalized coordinates
	glm::vec2 p;
	glm::vec2 e;

	glm::vec2 e1; //iteration e

	glm::vec2 borderWidth;

	glm::vec2 minSize;
	glm::vec2 maxSize;

	LAYOUT layout;

};

}

#endif

