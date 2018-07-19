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
	Client();
	virtual ~Client();
	//
	//virtual functions for the backend implementation
	//SetTranslation
	//SetLayer
	virtual Rectangle GetRect() const = 0;
};

class Container{
public:
	Container(); //root container
	Container(Container *);
	~Container();
	//
	Container *pParent;
	Container *pch; //First children
	Container *pnext; //Subsequent container in the parent container
	Container *pfocus; //Focused child (or this in case no children)
	Container *pPrevFocus; //Previously focused child (in case current focus is lost)
	Client *pclient;

	//float wscale, hscale;
	// = 1.0f: default size, scale with the number of containers
	// < 1.0f: smaller than default, leaving the unscaled parallel containers more space

	//Stack order: pnext is always below the current one
	
	/*enum{
		MODE_VSPLIT,
		MODE_HSPLIT,
		MODE_STACKED,
	} mode;*/
};

}


#endif

