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
	//Container * FindFocus();
	void Focus();
	Container * GetNext();
	Container * GetPrev();
	Container * GetParent();
	Container * GetFocus();

	//Stack order: pnext is always below the current one
	enum LAYOUT{
		LAYOUT_VSPLIT,
		LAYOUT_HSPLIT,
		//Title is rendered as tabs on the same row. Render titles inside a chamfered box.
		//stack mode: render titles vertically to the left?
		LAYOUT_STACKED,
		LAYOUT_TABBED,
	};
	void SetLayout(LAYOUT);

	Container *pParent;
	Container *pch; //First children
	Container *pnext; //Subsequent container in the parent container
	std::deque<Container *> focusQueue;

	Client *pclient;

	glm::vec2 scale;
	// = 1.0f: default size, scale with the number of containers
	// < 1.0f: smaller than default, leaving the unscaled parallel containers more space

	//absolute normalized coordinates
	glm::vec2 p;
	glm::vec2 e;

	glm::vec2 borderWidth;

	LAYOUT layout;

};

}

#endif

