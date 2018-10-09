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
	virtual void Focus() = 0;
	//Resize
	//SetLayer
	//virtual Rectangle GetRect() const = 0;
	Rectangle rect; //pixel coordinates of the client
	//TODO: scale; //scaling of the client inside the container
	class Container *pcontainer;
};

class Container{
public:
	Container(); //root container
	Container(Container *);
	~Container();
	void SetTranslation(glm::vec2, glm::vec2);
	void Remove();
	void Focus();
	Container * GetNext();
	Container * GetPrev();
	Container * GetParent();
	Container * GetFocus();

	glm::vec2 GetMinSize() const;
	//glm::vec2 GetOverlap(glm::vec2) const;
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

