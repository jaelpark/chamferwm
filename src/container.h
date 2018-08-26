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
	Client();
	virtual ~Client();
	//
	//virtual functions for the backend implementation
	virtual void SetTranslation(glm::vec2, glm::vec2) = 0;
	//Resize
	//SetLayer
	//virtual Rectangle GetRect() const = 0;
	Rectangle rect;
	//TODO: scale; //scaling of the client inside the container
};

/*class RootClient{
public:
	RootClient(uint, uint);
	~RootClient();
};
*/
class Container{
public:
	Container(); //root container
	Container(Container *);
	~Container();
	void SetTranslation(glm::vec2, glm::vec2);
	void Assign(Client *);

	Container *pParent;
	Container *pch; //First children
	Container *pnext; //Subsequent container in the parent container
	Container *pfocus; //Focused child (or this in case no children)
	Container *pPrevFocus; //Previously focused child (in case current focus is lost)
	Client *pclient;

	glm::vec2 scale;
	// = 1.0f: default size, scale with the number of containers
	// < 1.0f: smaller than default, leaving the unscaled parallel containers more space

	//absolute normalized coordinates
	glm::vec2 p;
	glm::vec2 e;

	//uint borderWidth; //0.5x the space allocated between two windows

	//Stack order: pnext is always below the current one
	enum{
		MODE_VSPLIT,
		MODE_HSPLIT,
		MODE_STACKED,
	} mode;
};

}


#endif

