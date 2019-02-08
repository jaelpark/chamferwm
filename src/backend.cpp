#include "main.h"
#include "container.h"
#include "backend.h"
#include <cstdlib>
#include <algorithm>

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>
//#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_util.h>

//#include <X11/X.h>
typedef xcb_atom_t Atom;
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <cerrno>

namespace Backend{

/*inline uint GetModMask1(uint keysym, xcb_key_symbols_t *psymbols, xcb_get_modifier_mapping_reply_t *pmodmapReply){
	xcb_keycode_t *pcodes = xcb_key_symbols_get_keycode(psymbols,keysym);
	if(!pcodes)
		return 0;

	xcb_keycode_t *pmodmap = xcb_get_modifier_mapping_keycodes(pmodmapReply);

	for(uint m = 0; m < 8; ++m)
		for(uint i = 0; i < pmodmapReply->keycodes_per_modifier; ++i){
			xcb_keycode_t mcode = pmodmap[m*pmodmapReply->keycodes_per_modifier+i];
			for(xcb_keycode_t *pcode = pcodes; *pcode; ++pcode){
				if(*pcode != mcode)
					continue;

				free(pcodes);
				return (1<<m);
			}
		}

	return 0;
}

static uint GetModMask(xcb_connection_t *pcon, uint keysym, xcb_key_symbols_t *psymbols){
	xcb_flush(pcon);

	xcb_get_modifier_mapping_cookie_t cookie = xcb_get_modifier_mapping(pcon);
	xcb_get_modifier_mapping_reply_t *pmodmapReply = xcb_get_modifier_mapping_reply(pcon,cookie,0);
	if(!pmodmapReply)
		return 0;

	uint mask = GetModMask1(keysym,psymbols,pmodmapReply);
	free(pmodmapReply);

	return mask;
}*/

static xcb_keycode_t SymbolToKeycode(xcb_keysym_t symbol, xcb_key_symbols_t *psymbols){
	xcb_keycode_t *pkey = xcb_key_symbols_get_keycode(psymbols,symbol);
	if(!pkey)
		return 0;

	xcb_keycode_t key = *pkey;
	free(pkey);

	return key;
}

BackendProperty::BackendProperty(PROPERTY_TYPE _type) : type(_type){
	//
}

BackendProperty::~BackendProperty(){
	//
}

BackendStringProperty::BackendStringProperty(const char *_pstr) : BackendProperty(PROPERTY_TYPE_STRING), pstr(_pstr){
	//
}

BackendStringProperty::~BackendStringProperty(){
	//
}

BackendContainerProperty::BackendContainerProperty(WManager::Container *_pcontainer) : BackendProperty(PROPERTY_TYPE_CLIENT), pcontainer(_pcontainer){
	//
}

BackendContainerProperty::~BackendContainerProperty(){
	//
}

BackendPixmapProperty::BackendPixmapProperty(xcb_pixmap_t _pixmap) : BackendProperty(PROPERTY_TYPE_PIXMAP), pixmap(_pixmap){
	//
}

BackendPixmapProperty::~BackendPixmapProperty(){
	//
}

BackendEvent::BackendEvent(){
	//
}

BackendEvent::~BackendEvent(){
	//
}

BackendKeyBinder::BackendKeyBinder(){
	//
}

BackendKeyBinder::~BackendKeyBinder(){
	//
}

BackendInterface::BackendInterface(){
	//
}

BackendInterface::~BackendInterface(){
	//
}

X11Event::X11Event(xcb_generic_event_t *_pevent, const X11Backend *_pbackend) : BackendEvent(), pevent(_pevent), pbackend(_pbackend){
	//
}

X11Event::~X11Event(){
	//
}

X11KeyBinder::X11KeyBinder(xcb_key_symbols_t *_psymbols, X11Backend *_pbackend) : BackendKeyBinder(), psymbols(_psymbols), pbackend(_pbackend){
	//
}

X11KeyBinder::~X11KeyBinder(){
	//
}

void X11KeyBinder::BindKey(uint symbol, uint mask, uint keyId){
	xcb_keycode_t keycode = SymbolToKeycode(symbol,psymbols);
	xcb_grab_key(pbackend->pcon,1,pbackend->pscr->root,mask,keycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	X11Backend::KeyBinding binding;
	binding.keycode = keycode;
	binding.mask = mask;
	binding.keyId = keyId;
	pbackend->keycodes.push_back(binding);
}

X11Client::X11Client(WManager::Container *pcontainer, const CreateInfo *pcreateInfo) : Client(pcontainer), window(pcreateInfo->window), pbackend(pcreateInfo->pbackend), flags(0){
	uint values[1] = {XCB_EVENT_MASK_ENTER_WINDOW|XCB_EVENT_MASK_PROPERTY_CHANGE|XCB_EVENT_MASK_FOCUS_CHANGE};
	xcb_change_window_attributes(pbackend->pcon,window,XCB_CW_EVENT_MASK,values);

	//if(!(pcontainer->flags & WManager::Container::FLAG_FLOATING))
	if((pcreateInfo->mode == CreateInfo::CREATE_CONTAINED && !(pcontainer->flags & WManager::Container::FLAG_FLOATING)) ||
		(pcreateInfo->mode == CreateInfo::CREATE_AUTOMATIC && !pcreateInfo->prect))
		UpdateTranslation();
	else rect = *pcreateInfo->prect;

	if(pcreateInfo->mode != CreateInfo::CREATE_AUTOMATIC)
		xcb_map_window(pbackend->pcon,window);
}

X11Client::~X11Client(){
	//
}

void X11Client::UpdateTranslation(){
	if(flags & FLAG_UNMAPPING)
		return; //if the window is about to be destroyed, do not attempt to adjust the surface of anything
	glm::vec4 screen(pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels,pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels);
	glm::vec2 aspect = glm::vec2(1.0,screen.x/screen.y);
	//glm::vec4 coord = glm::vec4(pcontainer->p+pcontainer->borderWidth*aspect,pcontainer->e-2.0f*pcontainer->borderWidth*aspect)*screen;
	glm::vec4 coord = glm::vec4(pcontainer->p,pcontainer->e)*screen;
	if(!(pcontainer->flags & WManager::Container::FLAG_FULLSCREEN))
		coord += glm::vec4(pcontainer->borderWidth*aspect,-2.0f*pcontainer->borderWidth*aspect)*screen;
	rect = (WManager::Rectangle){coord.x,coord.y,coord.z,coord.w};

	uint values[4] = {rect.x,rect.y,rect.w,rect.h};
	xcb_configure_window(pbackend->pcon,window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,values);

	xcb_flush(pbackend->pcon);

	//Virtual function gets called only if the compositor client class has been initialized.
	//In case compositor client class hasn't been initialized yet, this will be taken care of
	//later on subsequent constructor calls.
	AdjustSurface1();
}

void X11Client::UpdateTranslation(const WManager::Rectangle *prect){
	if(prect->w != rect.w || prect->h != rect.h){
		rect = *prect;
		AdjustSurface1();
	}else rect = *prect;
}

bool X11Client::ProtocolSupport(xcb_atom_t atom){
	xcb_get_property_cookie_t propertyCookie = xcb_icccm_get_wm_protocols(pbackend->pcon,window,pbackend->atoms[X11Backend::ATOM_WM_PROTOCOLS]);
	xcb_icccm_get_wm_protocols_reply_t protocols;
	if(xcb_icccm_get_wm_protocols_reply(pbackend->pcon,propertyCookie,&protocols,0) != 1)
		return false;

	bool support = false;
	for(uint i = 0; i < protocols.atoms_len; ++i)
		if(protocols.atoms[i] == atom){
			support = true;
			break;
		}
	//
	xcb_icccm_get_wm_protocols_reply_wipe(&protocols);
	return support;
}

void X11Client::Kill(){
	if(!ProtocolSupport(pbackend->atoms[X11Backend::ATOM_WM_DELETE_WINDOW])){
		xcb_destroy_window(pbackend->pcon,window);
		return;
	}

	char buffer[32];

	xcb_client_message_event_t *pev = (xcb_client_message_event_t*)buffer;
	pev->response_type = XCB_CLIENT_MESSAGE;
	pev->format = 32;
	pev->window = window;
	pev->type = pbackend->atoms[X11Backend::ATOM_WM_PROTOCOLS];
	pev->data.data32[0] = pbackend->atoms[X11Backend::ATOM_WM_DELETE_WINDOW];
	pev->data.data32[1] = XCB_CURRENT_TIME;

	xcb_send_event(pbackend->pcon,false,window,XCB_EVENT_MASK_NO_EVENT,buffer);
	xcb_flush(pbackend->pcon);
}

X11Container::X11Container(X11Backend *_pbackend) : WManager::Container(), pbackend(_pbackend){
	//
}

X11Container::X11Container(WManager::Container *_pParent, const WManager::Container::Setup &_setup, X11Backend *_pbackend) : WManager::Container(_pParent,_setup), pbackend(_pbackend){
	//
}

X11Container::~X11Container(){
	//
}

void X11Container::Focus1(){
	const xcb_window_t *pfocusWindow;
	if(pclient){
		X11Client *pclient11 = dynamic_cast<X11Client *>(pclient);
		pfocusWindow = &pclient11->window;

	}else pfocusWindow = &pbackend->ewmh_window;

	xcb_set_input_focus(pbackend->pcon,XCB_INPUT_FOCUS_POINTER_ROOT,*pfocusWindow,pbackend->lastTime);
	xcb_change_property(pbackend->pcon,XCB_PROP_MODE_REPLACE,pbackend->pscr->root,pbackend->ewmh._NET_ACTIVE_WINDOW,XCB_ATOM_WINDOW,32,1,pfocusWindow);

	xcb_flush(pbackend->pcon);
}

void X11Container::Stack1(){
	((X11Backend*)pbackend)->StackClients(); //hack, bypass const qualifier
}

void X11Container::Fullscreen1(){
	if(!pclient)
		return;
	X11Client *pclient11 = dynamic_cast<X11Client *>(pclient);
	if(flags & FLAG_FULLSCREEN){
		xcb_change_property(pbackend->pcon,XCB_PROP_MODE_APPEND,pclient11->window,pbackend->ewmh._NET_WM_STATE,XCB_ATOM_ATOM,32,1,&pbackend->ewmh._NET_WM_STATE_FULLSCREEN);

		//pclient11->SetFullscreen1(true);
	}else{
		xcb_grab_server(pbackend->pcon);
		xcb_get_property_cookie_t propertyCookie = xcb_get_property(pbackend->pcon,false,pclient11->window,pbackend->ewmh._NET_WM_STATE,XCB_GET_PROPERTY_TYPE_ANY,0,4096);
		xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pbackend->pcon,propertyCookie,0);
		if(!propertyReply){
			xcb_ungrab_server(pbackend->pcon);
			return;
		}
		uint l = xcb_get_property_value_length(propertyReply);
		if(l == 0){
			xcb_ungrab_server(pbackend->pcon);
			return;
		}
		uint s = 8*l/propertyReply->format;
		xcb_atom_t *patoms = (xcb_atom_t*)xcb_get_property_value(propertyReply);
		xcb_atom_t *pNewAtoms = new xcb_atom_t[s];
		uint n = 0;
		for(uint i = 0; i < s; ++i){
			if(patoms[i] != pbackend->ewmh._NET_WM_STATE_FULLSCREEN)
				pNewAtoms[n++] = patoms[i];
		}
		xcb_change_property(pbackend->pcon,XCB_PROP_MODE_REPLACE,pclient11->window,pbackend->ewmh._NET_WM_STATE,XCB_ATOM_ATOM,32,n,pNewAtoms);

		delete []pNewAtoms;
		free(propertyReply);
		xcb_ungrab_server(pbackend->pcon);

		//pclient11->SetFullscreen1(false);
	}

	xcb_flush(pbackend->pcon);
}

X11Backend::X11Backend() : lastTime(XCB_CURRENT_TIME){
	//
}

X11Backend::~X11Backend(){
	//
}

bool X11Backend::QueryExtension(const char *pname, sint *pfirstEvent, sint *pfirstError) const{
	xcb_generic_error_t *perr;
	xcb_query_extension_cookie_t queryExtCookie = xcb_query_extension(pcon,strlen(pname),pname);
	xcb_query_extension_reply_t *pqueryExtReply = xcb_query_extension_reply(pcon,queryExtCookie,&perr);
	if(!pqueryExtReply || perr)
		return false;
	*pfirstEvent = pqueryExtReply->first_event;
	*pfirstError = pqueryExtReply->first_error;
	free(pqueryExtReply);
	return true;
}

xcb_atom_t X11Backend::GetAtom(const char *pstr) const{
	xcb_generic_error_t *perr;
	xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom(pcon,0,strlen(pstr),pstr);
	xcb_intern_atom_reply_t *patomReply = xcb_intern_atom_reply(pcon,atomCookie,&perr);
	if(!patomReply || perr)
		return 0;
	xcb_atom_t atom = patomReply->atom;
	free(patomReply);
	return atom;
}

void X11Backend::StackRecursiveAppendix(const WManager::Client *pclient){
	auto s = [&](auto &p)->bool{
		return pclient == p.first;
	};
	for(auto m = std::find_if(appendixQueue.begin(),appendixQueue.end(),s);
		m != appendixQueue.end(); m = std::find_if(m,appendixQueue.end(),s)){
		StackRecursiveAppendix((*m).second);

		X11Client *pclient11 = dynamic_cast<X11Client *>((*m).second);

		uint values[1] = {XCB_STACK_MODE_ABOVE};
		xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);

		m = appendixQueue.erase(m);
	}
}

void X11Backend::StackRecursive(const WManager::Container *pcontainer){
	for(WManager::Container *pcont : pcontainer->stackQueue){
		if(pcont->pclient){
			X11Client *pclient11 = dynamic_cast<X11Client *>(pcont->pclient);

			uint values[1] = {XCB_STACK_MODE_ABOVE};
			xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
		}
		StackRecursive(pcont);
	}

	if(!pcontainer->pclient)
		return;
	StackRecursiveAppendix(pcontainer->pclient);
}

void X11Backend::StackClients(){
	SortStackAppendix();

	const WManager::Container *proot = GetRoot();
	const std::vector<std::pair<const WManager::Client *, WManager::Client *>> *pstackAppendix = GetStackAppendix();

	appendixQueue.clear();
	for(auto &p : *pstackAppendix){
		if(p.first){
			appendixQueue.push_back(p);
			continue;
		}

		X11Client *pclient11 = dynamic_cast<X11Client *>(p.second);

		uint values[1] = {XCB_STACK_MODE_ABOVE};
		xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
	}
	StackRecursive(proot);
	for(auto &p : appendixQueue){ //stack the remaining (untransient) windows
		X11Client *pclient11 = dynamic_cast<X11Client *>(p.second);

		uint values[1] = {XCB_STACK_MODE_ABOVE};
		xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
	}
}

/*void X11Backend::HandleTimer() const{
	char buffer[32];

	xcb_client_message_event_t *pev = (xcb_client_message_event_t*)buffer;
	pev->response_type = XCB_CLIENT_MESSAGE;
	pev->format = 32;
	pev->window = ewmh_window;
	pev->type = atoms[X11Backend::ATOM_CHAMFER_ALARM];
	//pev->data.data32[0] = XCB_CURRENT_TIME;

	xcb_send_event(pcon,false,ewmh_window,XCB_EVENT_MASK_NO_EVENT,buffer);
	xcb_flush(pcon);
}*/

const char *X11Backend::patomStrs[ATOM_COUNT] = {
	//"CHAMFER_ALARM",
	"WM_PROTOCOLS","WM_DELETE_WINDOW","ESETROOT_PMAP_ID","_X_ROOTPMAP_ID"
};

Default::Default() : X11Backend(), pdragClient(0){
	//
	clock_gettime(CLOCK_MONOTONIC,&eventTimer);
	pollTimer.tv_sec = 0;
	pollTimer.tv_nsec = 0;
	//polling = false;
}

Default::~Default(){
	//sigprocmask(SIG_UNBLOCK,&signals,0);

	//cleanup
	xcb_destroy_window(pcon,ewmh_window);
	xcb_ewmh_connection_wipe(&ewmh);
	xcb_set_input_focus(pcon,XCB_NONE,XCB_INPUT_FOCUS_POINTER_ROOT,XCB_CURRENT_TIME);
	xcb_disconnect(pcon);
	xcb_flush(pcon);
}

void Default::Start(){
	sint scount;
	pcon = xcb_connect(0,&scount);
	if(xcb_connection_has_error(pcon))
		throw Exception("Failed to connect to X server.\n");

	const xcb_setup_t *psetup = xcb_get_setup(pcon);
	xcb_screen_iterator_t sm = xcb_setup_roots_iterator(psetup);
	for(sint i = 0; i < scount; ++i)
		xcb_screen_next(&sm);

	pscr = sm.data;
	if(!pscr)
		throw Exception("Screen unavailable.\n");

	DebugPrintf(stdout,"Screen size: %ux%u\n",pscr->width_in_pixels,pscr->height_in_pixels);
	//https://standards.freedesktop.org/wm-spec/wm-spec-1.3.html#idm140130317705584

	xcb_key_symbols_t *psymbols = xcb_key_symbols_alloc(pcon);

	exitKeycode = SymbolToKeycode(XK_E,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1|XCB_MOD_MASK_SHIFT,exitKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	
	X11KeyBinder binder(psymbols,this);
	DefineBindings(&binder); //TODO: an interface to define bindings? This is passed to config before calling SetupKeys

	xcb_flush(pcon);

	xcb_key_symbols_free(psymbols);

	uint values[2] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
		|XCB_EVENT_MASK_EXPOSURE
		|XCB_EVENT_MASK_PROPERTY_CHANGE,0}; //[2] for later use
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(pcon,pscr->root,XCB_CW_EVENT_MASK,values);
	xcb_generic_error_t *perr = xcb_request_check(pcon,cookie);

	xcb_flush(pcon);

	window = pscr->root;
	DebugPrintf(stdout,"Root id: %x\n",window);

	if(perr != 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Substructure redirection failed (%d). WM already present.\n",perr->error_code);
		throw Exception();
	}

	xcb_intern_atom_cookie_t *patomCookie = xcb_ewmh_init_atoms(pcon,&ewmh);
	if(!xcb_ewmh_init_atoms_replies(&ewmh,patomCookie,0))
		throw Exception("Failed to initialize EWMH atoms.\n");
	
	for(uint i = 0; i < ATOM_COUNT; ++i)
		atoms[i] = GetAtom(patomStrs[i]);
	
	const char wmName[] = "chamfer";

	//setup EWMH hints
	ewmh_window = xcb_generate_id(pcon);

	values[0] = 1;
	xcb_create_window(pcon,XCB_COPY_FROM_PARENT,ewmh_window,pscr->root,
		-1,-1,1,1,0,XCB_WINDOW_CLASS_INPUT_ONLY,XCB_COPY_FROM_PARENT,XCB_CW_OVERRIDE_REDIRECT,values);
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,ewmh_window,ewmh._NET_SUPPORTING_WM_CHECK,XCB_ATOM_WINDOW,32,1,&ewmh_window);
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,ewmh_window,ewmh._NET_WM_NAME,XCB_ATOM_STRING,8,strlen(wmName),wmName);

	//https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html#idm140130317705584
	const xcb_atom_t supportedAtoms[] = {
		ewmh._NET_WM_NAME,
		ewmh._NET_CLIENT_LIST,
		//TODO: _NET_CLIENT_LIST_STACKING
		ewmh._NET_CURRENT_DESKTOP,
		ewmh._NET_NUMBER_OF_DESKTOPS,
		ewmh._NET_DESKTOP_VIEWPORT,
		ewmh._NET_DESKTOP_GEOMETRY,
		ewmh._NET_ACTIVE_WINDOW
	};

	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_SUPPORTING_WM_CHECK,XCB_ATOM_WINDOW,32,1,&ewmh_window);
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_SUPPORTED,XCB_ATOM_ATOM,32,sizeof(supportedAtoms)/sizeof(supportedAtoms[0]),supportedAtoms);

	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_WM_NAME,XCB_ATOM_STRING,8,strlen(wmName),wmName);
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,0,0);
	values[0] = 0;
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CURRENT_DESKTOP,XCB_ATOM_CARDINAL,32,1,values);
	values[0] = 1;
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_NUMBER_OF_DESKTOPS,XCB_ATOM_CARDINAL,32,1,values);
	values[0] = 0;
	values[1] = 0;
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_DESKTOP_VIEWPORT,XCB_ATOM_CARDINAL,32,2,values);
	values[0] = pscr->width_in_pixels;
	values[1] = pscr->height_in_pixels;
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_DESKTOP_VIEWPORT,XCB_ATOM_CARDINAL,32,2,values);
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_ACTIVE_WINDOW,XCB_ATOM_WINDOW,32,1,&ewmh_window);

	xcb_map_window(pcon,ewmh_window);

	values[0] = XCB_STACK_MODE_BELOW;
	xcb_configure_window(pcon,ewmh_window,XCB_CONFIG_WINDOW_STACK_MODE,values);
}

sint Default::HandleEvent(){
	struct timespec currentTime;
	clock_gettime(CLOCK_MONOTONIC,&currentTime);
	
	/*if(timespec_diff(currentTime,pollTimer) >= 0.04f){
	//if(!polling){
		sint fd = xcb_get_file_descriptor(pcon);

		fd_set in;
		FD_ZERO(&in);
		FD_SET(fd,&in);
		
		//-> get 5s - (currentTime-eventTimer);
		struct timespec interval = {
			tv_sec: 5,
			tv_nsec: 0
		}, timeout, eventDiff;
		timespec_diff_ptr(currentTime,eventTimer,eventDiff);

		if(eventDiff.tv_sec >= interval.tv_sec){
			timeout.tv_sec = 0;
			timeout.tv_nsec = 0;
		}else timespec_diff_ptr(interval,eventDiff,timeout);

		sint sr = pselect(fd+1,&in,0,0,&timeout,0);
		if(sr == 0){
			//timer event
			TimerEvent();
			clock_gettime(CLOCK_MONOTONIC,&eventTimer);
			return 0;
		}else
		if(sr == -1){
			if(errno == EINTR)
				printf("signal!\n"); //TODO: actually handle it
		}
		
	}else{
		//There has been an event during the past two seconds. Poll xcb events until these
		//two seconds elapse, while also checking the callback timer.
		//This cumbersome mechanism is to ensure that the refresh interval matches that of the
		//application that sends the highest rate of damage events, thus preventing stutter.
		//Using select() alone seemingly stutters some of applications, while this keeps everything
		//smooth, and still makes callbacks possible without continued polling by the CPU.
		if(timespec_diff(currentTime,eventTimer) >= 5.0f){
			TimerEvent();
			clock_gettime(CLOCK_MONOTONIC,&eventTimer);
		}
	}*/

	//polling = false;

	sint result = 0;

	//for(xcb_generic_event_t *pevent = xcb_poll_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
	for(xcb_generic_event_t *pevent = xcb_wait_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
		//Event found, move to polling mode for some time.
		clock_gettime(CLOCK_MONOTONIC,&pollTimer);
		//polling = true;

		lastTime = XCB_CURRENT_TIME;

		//switch(pevent->response_type & ~0x80){
		switch(pevent->response_type & 0x7f){
		case XCB_CREATE_NOTIFY:{
			xcb_create_notify_event_t *pev = (xcb_create_notify_event_t*)pevent;

			WManager::Rectangle rect = {pev->x,pev->y,pev->width,pev->height};
			
			auto m = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == p.first;
			});
			if(m == configCache.end()){
				configCache.push_back(std::pair<xcb_window_t, WManager::Rectangle>(pev->window,rect));
				m = configCache.end()-1;
			}else (*m).second = rect;

			DebugPrintf(stdout,"create %x | %d,%d %ux%u\n",pev->window,pev->x,pev->y,pev->width,pev->height);
			}
			break;
		case XCB_CONFIGURE_REQUEST:{
			xcb_configure_request_event_t *pev = (xcb_configure_request_event_t*)pevent;

			//check if window already exists
			//further configuration should be blocked (for example Firefox on restore session)
			X11Client *pclient1 = FindClient(pev->window,MODE_MANUAL);
			if(pclient1 && !(pclient1->pcontainer->flags & WManager::Container::FLAG_FLOATING))
				break;

			auto mrect = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == p.first;
			});

			//TODO: allow x,y configuration if dock or desktop feature
			xcb_get_property_cookie_t propertyCookieNormalHints = xcb_icccm_get_wm_normal_hints(pcon,pev->window);
			xcb_get_property_cookie_t propertyCookieWindowType
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_WINDOW_TYPE,XCB_ATOM_ATOM,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookieWindowState
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_STATE,XCB_ATOM_ATOM,0,std::numeric_limits<uint32_t>::max());

			xcb_get_property_reply_t *propertyReplyWindowType
				= xcb_get_property_reply(pcon,propertyCookieWindowType,0);
			xcb_get_property_reply_t *propertyReplyWindowState
				= xcb_get_property_reply(pcon,propertyCookieWindowState,0);

			WManager::Rectangle rect = {
				pev->x,pev->y,pev->width,pev->height
			};

			bool allowPositionConfig = false;
			if(propertyReplyWindowType){
				uint l = xcb_get_property_value_length(propertyReplyWindowType);
				if(l > 0){
					xcb_atom_t *patom = (xcb_atom_t*)xcb_get_property_value(propertyReplyWindowType);
					allowPositionConfig = (patom[0] == ewmh._NET_WM_WINDOW_TYPE_DESKTOP || patom[0] == ewmh._NET_WM_WINDOW_TYPE_DOCK);
				}
				free(propertyReplyWindowType);
			}
			if(propertyReplyWindowState){
				uint l = xcb_get_property_value_length(propertyReplyWindowState);
				if(l > 0 && !allowPositionConfig){
					uint n = 8*l/propertyReplyWindowState->format;
					xcb_atom_t *patom = (xcb_atom_t*)xcb_get_property_value(propertyReplyWindowState);
					allowPositionConfig = any(ewmh._NET_WM_STATE_SKIP_TASKBAR,patom,n);
				}
				free(propertyReplyWindowState);
			}

			//center the window to screen, otherwise it might end up to the left upper corner
			if(!allowPositionConfig){
				rect.x = (pscr->width_in_pixels-rect.w)/2;
				rect.y = (pscr->height_in_pixels-rect.h)/2;
				pev->value_mask |= XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y;
			}
			
			if(mrect == configCache.end()){
				configCache.push_back(std::pair<xcb_window_t, WManager::Rectangle>(pev->window,rect));
				mrect = configCache.end()-1;
			}else (*mrect).second = rect;

			struct{
				uint16_t m;
				uint32_t v;
			} maskm[] = {
				{XCB_CONFIG_WINDOW_X,rect.x},
				{XCB_CONFIG_WINDOW_Y,rect.y},
				{XCB_CONFIG_WINDOW_WIDTH,rect.w},
				{XCB_CONFIG_WINDOW_HEIGHT,rect.h},
				{XCB_CONFIG_WINDOW_BORDER_WIDTH,pev->border_width},
				{XCB_CONFIG_WINDOW_SIBLING,pev->sibling},
				//{XCB_CONFIG_WINDOW_STACK_MODE,pev->stack_mode} //floating clients may request configuration, including stack mode, which ruins the stacking order we've already set
			};
			uint values[6];
			uint mask = 0;
			for(uint j = 0, mi = 0; j < 6; ++j)
				if(maskm[j].m & pev->value_mask){
					mask |= maskm[j].m;
					values[mi++] = maskm[j].v;
				}

			xcb_configure_window(pcon,pev->window,mask,values);

			if(pclient1)
				pclient1->UpdateTranslation(&rect);

			DebugPrintf(stdout,"configure request: %x | %d, %d, %u, %u\n",pev->window,pev->x,pev->y,pev->width,pev->height);
			}
			break;
		case XCB_MAP_REQUEST:{
			//TODO: use xprop to identify windows that don't behave as expected.
			xcb_map_request_event_t *pev = (xcb_map_request_event_t*)pevent;
			if(pev->window == ewmh_window){
				xcb_map_window(pcon,pev->window);
				break;
			}

			result = 1;
			
			//check if window already exists
			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(pclient1){
				xcb_map_window(pcon,pev->window);
				break;
			}

			WManager::Rectangle *prect = 0;
			X11Client *pbaseClient = 0;
			uint hintFlags = 0;

			xcb_get_property_cookie_t propertyCookieHints = xcb_icccm_get_wm_hints(pcon,pev->window);
			xcb_get_property_cookie_t propertyCookieNormalHints = xcb_icccm_get_wm_normal_hints(pcon,pev->window);
			xcb_get_property_cookie_t propertyCookieWindowType
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_WINDOW_TYPE,XCB_ATOM_ATOM,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookieWindowState
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_STATE,XCB_ATOM_ATOM,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookieStrut
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_STRUT_PARTIAL,XCB_ATOM_CARDINAL,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookieTransientFor
				= xcb_get_property(pcon,0,pev->window,XA_WM_TRANSIENT_FOR,XCB_ATOM_WINDOW,0,std::numeric_limits<uint32_t>::max());

			xcb_icccm_wm_hints_t hints;
			bool boolHints = xcb_icccm_get_wm_hints_reply(pcon,propertyCookieHints,&hints,0);

			xcb_size_hints_t sizeHints;
			bool boolSizeHints = xcb_icccm_get_wm_size_hints_reply(pcon,propertyCookieNormalHints,&sizeHints,0);
			xcb_get_property_reply_t *propertyReplyWindowType
				= xcb_get_property_reply(pcon,propertyCookieWindowType,0);
			xcb_get_property_reply_t *propertyReplyWindowState
				= xcb_get_property_reply(pcon,propertyCookieWindowState,0);
			//xcb_get_property_reply_t *propertyReplyStrut
				//= xcb_get_property_reply(pcon,propertyCookieStrut,0);
			xcb_get_property_reply_t *propertyReplyTransientFor
				= xcb_get_property_reply(pcon,propertyCookieTransientFor,0);

			bool allowPositionConfig = false;
			if(propertyReplyWindowType){
				uint l = xcb_get_property_value_length(propertyReplyWindowType);
				if(l > 0){
					xcb_atom_t *patom = (xcb_atom_t*)xcb_get_property_value(propertyReplyWindowType);
					allowPositionConfig = (patom[0] == ewmh._NET_WM_WINDOW_TYPE_DESKTOP || patom[0] == ewmh._NET_WM_WINDOW_TYPE_DOCK);
				}
			}
			if(propertyReplyWindowState){
				uint l = xcb_get_property_value_length(propertyReplyWindowState);
				if(l > 0 && !allowPositionConfig){
					uint n = 8*l/propertyReplyWindowState->format;
					xcb_atom_t *patom = (xcb_atom_t*)xcb_get_property_value(propertyReplyWindowState);
					allowPositionConfig = any(ewmh._NET_WM_STATE_SKIP_TASKBAR,patom,n);
				}
			}

			auto mrect = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == p.first;
			});

			if(boolHints){
				if(hints.flags & XCB_ICCCM_WM_HINT_INPUT && !hints.input)
					hintFlags |= X11Client::CreateInfo::HINT_NO_INPUT;
			}
			if(boolSizeHints){
				WManager::Rectangle rect;
				if(mrect == configCache.end()){
					configCache.push_back(std::pair<xcb_window_t, WManager::Rectangle>(pev->window,rect));
					mrect = configCache.end()-1;
				}

				if(sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE &&
				sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE &&
				(sizeHints.min_width == sizeHints.max_width ||
				sizeHints.min_height == sizeHints.max_height)){
					//
					(*mrect).second.w = sizeHints.min_width;
					(*mrect).second.h = sizeHints.min_height;
					prect = &(*mrect).second;
				}else
				if(sizeHints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE || sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE){
					(*mrect).second.w = sizeHints.width;
					(*mrect).second.h = sizeHints.height;
				}

				rect.x = (pscr->width_in_pixels-rect.w)/2;
				rect.y = (pscr->height_in_pixels-rect.h)/2;
				if((sizeHints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION || sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION) && allowPositionConfig){
					(*mrect).second.x = sizeHints.x;
					(*mrect).second.y = sizeHints.y;
				}
			}
			if(propertyReplyWindowType){
				//https://specifications.freedesktop.org/wm-spec/1.3/ar01s05.html
				uint l = xcb_get_property_value_length(propertyReplyWindowType);
				if(l > 0){
					//uint n = 8*l/propertyReplyWindowType->format;
					xcb_atom_t *patom = (xcb_atom_t*)xcb_get_property_value(propertyReplyWindowType);
					if(patom[0] == ewmh._NET_WM_WINDOW_TYPE_DIALOG ||
						patom[0] == ewmh._NET_WM_WINDOW_TYPE_DOCK ||
						patom[0] == ewmh._NET_WM_WINDOW_TYPE_DESKTOP ||
						patom[0] == ewmh._NET_WM_WINDOW_TYPE_TOOLBAR ||
						patom[0] == ewmh._NET_WM_WINDOW_TYPE_MENU ||
						patom[0] == ewmh._NET_WM_WINDOW_TYPE_UTILITY ||
						patom[0] == ewmh._NET_WM_WINDOW_TYPE_SPLASH){
						if(!prect)
							prect = mrect != configCache.end()?&(*mrect).second:0;

						hintFlags |= 
							(patom[0] == ewmh._NET_WM_WINDOW_TYPE_DESKTOP?X11Client::CreateInfo::HINT_DESKTOP:0)|
							(patom[0] == ewmh._NET_WM_WINDOW_TYPE_DOCK?X11Client::CreateInfo::HINT_ABOVE:0);
					}
				}
				free(propertyReplyWindowType);
			}
			if(propertyReplyWindowState){
				uint l = xcb_get_property_value_length(propertyReplyWindowState);
				if(l > 0){
					uint n = 8*l/propertyReplyWindowState->format;
					xcb_atom_t *patom = (xcb_atom_t*)xcb_get_property_value(propertyReplyWindowState);

					if(any(ewmh._NET_WM_STATE_MODAL,patom,n) || any(ewmh._NET_WM_STATE_SKIP_TASKBAR,patom,n)){
						if(!prect)
							prect = mrect != configCache.end()?&(*mrect).second:0;

						hintFlags |= 
							(any(ewmh._NET_WM_STATE_ABOVE,patom,n)?X11Client::CreateInfo::HINT_ABOVE:0)|
							(any(ewmh._NET_WM_STATE_BELOW,patom,n)?X11Client::CreateInfo::HINT_DESKTOP:0)|
							(any(ewmh._NET_WM_STATE_FULLSCREEN,patom,n)?X11Client::CreateInfo::HINT_FULLSCREEN:0);
					}
				}
				free(propertyReplyWindowState);
			}
			if(hintFlags & X11Client::CreateInfo::HINT_DESKTOP){
				//TODO: get strut
				//uint32_t *pstrut = (uint32_t*)xcb_get_property_value(propertyReplyStrut);
				//TODO: set window x,y
				/*if(pstrut[2] > 0 && pstrut[3] == 0)
					; //place top of the desktop, y = 0
				else
				if(pstrut[3] > 0 && pstrut[2] == 0)
					; //place bottom of the desktop, y = max-h*/
			}
			if(propertyReplyTransientFor){
				xcb_window_t *pbaseWindow = (xcb_window_t*)xcb_get_property_value(propertyReplyTransientFor);
				pbaseClient = FindClient(*pbaseWindow,MODE_UNDEFINED);
				free(propertyReplyTransientFor);
			}

			xcb_get_property_cookie_t propertyCookie1[2];
			xcb_get_property_reply_t *propertyReply1[2];

			propertyCookie1[0] = xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_NAME,XCB_GET_PROPERTY_TYPE_ANY,0,128);
			propertyCookie1[1] = xcb_get_property(pcon,0,pev->window,XCB_ATOM_WM_CLASS,XCB_GET_PROPERTY_TYPE_ANY,0,128);
			for(uint i = 0; i < 2; ++i)
				propertyReply1[i] = xcb_get_property_reply(pcon,propertyCookie1[i],0);
			BackendStringProperty wmName((const char *)xcb_get_property_value(propertyReply1[0]));
			BackendStringProperty wmClass((const char *)xcb_get_property_value(propertyReply1[1]));

			static WManager::Client dummyClient(0); //base client being unavailable means that the client is stacked on top of everything else

			X11Client::CreateInfo createInfo;
			createInfo.window = pev->window;
			createInfo.prect = prect;
			createInfo.pstackClient =
				!(hintFlags & X11Client::CreateInfo::HINT_DESKTOP)?
					((pbaseClient && !(hintFlags & X11Client::CreateInfo::HINT_ABOVE))?
						pbaseClient
					:&dummyClient)
				:0;
			createInfo.pbackend = this;
			createInfo.mode = X11Client::CreateInfo::CREATE_CONTAINED;
			createInfo.hints = hintFlags;
			createInfo.pwmName = &wmName;
			createInfo.pwmClass = &wmClass;
			X11Client *pclient = SetupClient(&createInfo);
			if(!pclient)
				break;
			clients.push_back(std::pair<X11Client *, MODE>(pclient,MODE_MANUAL));

			StackClients();

			if(hintFlags & X11Client::CreateInfo::HINT_FULLSCREEN)
				SetFullscreen(pclient,true);

			for(uint i = 0; i < 2; ++i)
				free(propertyReply1[i]);

			netClientList.clear();
			for(auto &p : clients)
				netClientList.push_back(p.first->window);
			xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,netClientList.size(),netClientList.data());

			xcb_grab_button(pcon,0,pev->window,XCB_EVENT_MASK_BUTTON_PRESS,//|XCB_EVENT_MASK_BUTTON_RELEASE,
				XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC,pscr->root,XCB_NONE,1,XCB_MOD_MASK_1);
	
			//check fullscreen
		
			DebugPrintf(stdout,"map request, %x\n",pev->window);
			}
			break;
		case XCB_CONFIGURE_NOTIFY:{
			xcb_configure_notify_event_t *pev = (xcb_configure_notify_event_t*)pevent;

			WManager::Rectangle rect = {pev->x,pev->y,pev->width,pev->height};
			
			auto m = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == p.first;
			});
			if(m == configCache.end()){
				configCache.push_back(std::pair<xcb_window_t, WManager::Rectangle>(pev->window,rect));
				m = configCache.end()-1;
			}else (*m).second = rect;

			X11Client *pclient1 = FindClient(pev->window,MODE_AUTOMATIC);
			if(!pclient1)
				break;

			pclient1->UpdateTranslation(&rect);

			DebugPrintf(stdout,"configure to %d,%d %ux%u, %x\n",pev->x,pev->y,pev->width,pev->height,pev->window);
			}
			break;
		case XCB_MAP_NOTIFY:{
			xcb_map_notify_event_t *pev = (xcb_map_notify_event_t*)pevent;
			if(pev->window == ewmh_window)
				break;

			result = 1;

			//check if window already exists
			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(pclient1)
				break;
			
			X11Client *pbaseClient = 0;

			xcb_get_property_cookie_t propertyCookieTransientFor
				= xcb_get_property(pcon,0,pev->window,XA_WM_TRANSIENT_FOR,XCB_ATOM_WINDOW,0,std::numeric_limits<uint32_t>::max());

			xcb_get_property_reply_t *propertyReplyTransientFor
				= xcb_get_property_reply(pcon,propertyCookieTransientFor,0);

			if(propertyReplyTransientFor){
				xcb_window_t *pbaseWindow = (xcb_window_t*)xcb_get_property_value(propertyReplyTransientFor);
				pbaseClient = FindClient(*pbaseWindow,MODE_UNDEFINED);
				free(propertyReplyTransientFor);
			}

			auto m = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == p.first;
			});
			if(m == configCache.end()){
				//it might be the case that no configure notification was received
				xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pcon,pev->window);
				xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pcon,geometryCookie,0);
				if(!pgeometryReply)
					break; //happens sometimes on high rate of events
				WManager::Rectangle rect = {pgeometryReply->x,pgeometryReply->y,pgeometryReply->width,pgeometryReply->height};
				free(pgeometryReply);

				configCache.push_back(std::pair<xcb_window_t, WManager::Rectangle>(pev->window,rect));
				m = configCache.end()-1;

			}
			WManager::Rectangle *prect = &(*m).second;
			if(prect->x+prect->w <= 1 || prect->y+prect->h <= 1)
				break; //hack: don't manage, this will mess the compositor

			static WManager::Client dummyClient(0);

			X11Client::CreateInfo createInfo;
			createInfo.window = pev->window;
			createInfo.prect = prect;
			createInfo.pstackClient = pbaseClient?pbaseClient:&dummyClient;
			createInfo.pbackend = this;
			createInfo.mode = X11Client::CreateInfo::CREATE_AUTOMATIC;
			createInfo.hints = 0;
			createInfo.pwmName = 0;
			createInfo.pwmClass = 0;
			X11Client *pclient = SetupClient(&createInfo);
			if(!pclient)
				break;
			clients.push_back(std::pair<X11Client *, MODE>(pclient,MODE_AUTOMATIC));

			StackClients();

			netClientList.clear();
			for(auto &p : clients)
				netClientList.push_back(p.first->window);
			xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,netClientList.size(),netClientList.data());

			DebugPrintf(stdout,"map notify, %x\n",pev->window);
			}
			break;
		case XCB_UNMAP_NOTIFY:{
			//
			xcb_unmap_notify_event_t *pev = (xcb_unmap_notify_event_t*)pevent;

			auto m = std::find_if(clients.begin(),clients.end(),[&](auto &p)->bool{
				return p.first->window == pev->window;
			});
			if(m == clients.end())
				break;

			result = 1;

			(*m).first->flags |= X11Client::FLAG_UNMAPPING;
			unmappingQueue.push_back((*m).first);

			std::iter_swap(m,clients.end()-1);
			clients.pop_back();

			netClientList.clear();
			for(auto &p : clients)
				netClientList.push_back(p.first->window);
			xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,netClientList.size(),netClientList.data());

			DebugPrintf(stdout,"unmap notify %x\n",pev->window);
			}
			break;
		case XCB_PROPERTY_NOTIFY:{
			xcb_property_notify_event_t *pev = (xcb_property_notify_event_t*)pevent;
			lastTime = pev->time;
			if(pev->state == XCB_PROPERTY_DELETE)
				break;

			result = 1;

			if(pev->window == pscr->root){
				if(pev->atom == atoms[ATOM_ESETROOT_PMAP_ID]){
					xcb_get_property_cookie_t propertyCookie = xcb_get_property(pcon,0,pev->window,atoms[ATOM_ESETROOT_PMAP_ID],XCB_GET_PROPERTY_TYPE_ANY,0,128);
					xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pcon,propertyCookie,0);
					if(!propertyReply)
						break;

					BackendPixmapProperty prop(*(xcb_pixmap_t*)xcb_get_property_value(propertyReply));
					PropertyChange(0,PROPERTY_ID_PIXMAP,&prop);

					free(propertyReply);
				}
				break;
			}

			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(!pclient1)
				break;

			if(pev->atom == XCB_ATOM_WM_NAME){
				xcb_get_property_cookie_t propertyCookie = xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_NAME,XCB_GET_PROPERTY_TYPE_ANY,0,128);
				xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pcon,propertyCookie,0);
				if(!propertyReply)
					break; //TODO: get legacy XCB_ATOM_WM_NAME
				BackendStringProperty prop((const char *)xcb_get_property_value(propertyReply));
				PropertyChange(pclient1,PROPERTY_ID_WM_NAME,&prop);

				free(propertyReply);

			}else
			if(pev->atom == XCB_ATOM_WM_CLASS){
				xcb_get_property_cookie_t propertyCookie = xcb_get_property(pcon,0,pev->window,XCB_ATOM_WM_CLASS,XCB_GET_PROPERTY_TYPE_ANY,0,128);
				xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pcon,propertyCookie,0);
				if(!propertyReply)
					break;
				BackendStringProperty prop((const char *)xcb_get_property_value(propertyReply));
				PropertyChange(pclient1,PROPERTY_ID_WM_CLASS,&prop);

				free(propertyReply);
			}else
			if(pev->atom == XA_WM_TRANSIENT_FOR){
				xcb_get_property_cookie_t propertyCookie = xcb_get_property(pcon,0,pev->window,XA_WM_TRANSIENT_FOR,XCB_ATOM_WINDOW,0,std::numeric_limits<uint32_t>::max());

				xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pcon,propertyCookie,0);
				if(!propertyReply)
					break;
				//
				xcb_window_t *pbaseWindow = (xcb_window_t*)xcb_get_property_value(propertyReply);
				X11Client *pbaseClient = FindClient(*pbaseWindow,MODE_UNDEFINED);
				if(pbaseClient){
					BackendContainerProperty prop(pbaseClient->pcontainer);
					PropertyChange(pclient1,PROPERTY_ID_TRANSIENT_FOR,&prop);
				}

				free(propertyReply);
			}

			}
			break;
		case XCB_CLIENT_MESSAGE:{
			xcb_client_message_event_t *pev = (xcb_client_message_event_t*)pevent;
			/*if(pev->window == ewmh_window){
				if(pev->type == atoms[X11Backend::ATOM_CHAMFER_ALARM])
					TimerEvent();
				break;
			}*/
			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(!pclient1)
				break;

			if(pev->type == ewmh._NET_WM_STATE){
				if(pev->data.data32[1] == ewmh._NET_WM_STATE_FULLSCREEN){
					switch(pev->data.data32[0]){
					case 2://ewmh._NET_WM_STATE_TOGGLE)
						SetFullscreen(pclient1,(pclient1->pcontainer->flags & WManager::Container::FLAG_FULLSCREEN) == 0);
						break;
					case 1://ewmh._NET_WM_STATE_ADD)
						SetFullscreen(pclient1,true);
						break;
					case 0://ewmh._NET_WM_STATE_REMOVE)
						SetFullscreen(pclient1,false);
						break;
					}

				}else
				if(pev->data.data32[1] == ewmh._NET_WM_STATE_DEMANDS_ATTENTION){
					printf("urgency!\n");
				}
			}
			//_NET_WM_STATE
			//_NET_WM_STATE_FULLSCREEN, _NET_WM_STATE_DEMANDS_ATTENTION
			result = 1;
			}
			break;
		case XCB_KEY_PRESS:{
			xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
			lastTime = pev->time;
			if(pev->state == (XCB_MOD_MASK_1|XCB_MOD_MASK_SHIFT) && pev->detail == exitKeycode){
				free(pevent);
				return -1;
			//
			}else
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,true);
					result = 1;
					break;
				}
			}
			}
			break;
		case XCB_KEY_RELEASE:{
			xcb_key_release_event_t *pev = (xcb_key_release_event_t*)pevent;
			lastTime = pev->time;
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,false);
					result = 1;
					break;
				}
			}
			}
			break;
		case XCB_BUTTON_PRESS:{
			xcb_button_press_event_t *pev = (xcb_button_press_event_t*)pevent;
			//
			X11Client *pclient11 = FindClient(pev->event,MODE_UNDEFINED);
			if(!pclient11 || !(pclient11->pcontainer->flags & WManager::Container::FLAG_FLOATING))
				break;

			pdragClient = pclient11;
			dragClientX = pev->event_x;
			dragClientY = pev->event_y;

			xcb_grab_pointer(pcon,0,pscr->root,XCB_EVENT_MASK_BUTTON_RELEASE|XCB_EVENT_MASK_POINTER_MOTION,
				XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC,pscr->root,XCB_NONE,XCB_CURRENT_TIME);
			}
			break;
		case XCB_BUTTON_RELEASE:{
			//xcb_button_release_event_t *pev = (xcb_button_release_event_t*)pevent;

			pdragClient = 0;
			xcb_ungrab_pointer(pcon,XCB_CURRENT_TIME);
			}
			break;
		case XCB_MOTION_NOTIFY:{
			xcb_motion_notify_event_t *pev = (xcb_motion_notify_event_t*)pevent;

			sint values[2] = {pev->event_x-dragClientX,pev->event_y-dragClientY};
			xcb_configure_window(pcon,pdragClient->window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y,values);

			WManager::Rectangle rect = {values[0],values[1],pdragClient->rect.w,pdragClient->rect.h};
			pdragClient->UpdateTranslation(&rect);

			//printf("*** motion %d,%d\n",pev->event_x,pev->event_y);
			}
			break;
		case XCB_FOCUS_IN:{
			xcb_focus_in_event_t *pev = (xcb_focus_in_event_t*)pevent;
			printf("*** focus %x\n",pev->event);
			}
			break;
		case XCB_ENTER_NOTIFY:{
			xcb_enter_notify_event_t *pev = (xcb_enter_notify_event_t*)pevent;
			lastTime = pev->time;

			/*X11Client *pclient1 = FindClient(pev->event,MODE_UNDEFINED);
			if(!pclient1)
				break;*/

			DebugPrintf(stdout,"enter %x\n",pev->event);
			}
			break;
		case XCB_MAPPING_NOTIFY:
			//keyboard related stuff
			DebugPrintf(stdout,"mapping\n");
			break;
		case XCB_DESTROY_NOTIFY:{
			xcb_destroy_notify_event_t *pev = (xcb_destroy_notify_event_t*)pevent;
			DebugPrintf(stdout,"destroy notify, %x\n",pev->window);

			/*configCache.erase(std::remove_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return p.first == pev->window;
			}));*/
			//
			}
			break;
		case 0:
			DebugPrintf(stdout,"Invalid event\n");
			break;
		default:
			DebugPrintf(stdout,"default event: %u\n",pevent->response_type & 0x7f);

			X11Event event11(pevent,this);
			EventNotify(&event11);

			result = 1;
			break;
		}
		
		free(pevent);
		xcb_flush(pcon);
	}

	//Destroy the clients here, after the event queue has been cleared. This is to ensure that no already destroyed
	//client is attempted to be readjusted.
	for(X11Client *pclient : unmappingQueue)
		DestroyClient(pclient);
	unmappingQueue.clear();

	if(xcb_connection_has_error(pcon)){
		DebugPrintf(stderr,"X server connection lost\n");
		return -1;
	}

	return result;
}

X11Client * Default::FindClient(xcb_window_t window, MODE mode) const{
	auto m = std::find_if(clients.begin(),clients.end(),[&](auto &p)->bool{
		return p.first->window == window && (mode == MODE_UNDEFINED || p.second == mode);
	});
	if(m == clients.end())
		return 0;
	return (*m).first;
}

DebugClient::DebugClient(WManager::Container *pcontainer, const DebugClient::CreateInfo *pcreateInfo) : Client(pcontainer), pbackend(pcreateInfo->pbackend){
	UpdateTranslation();
}

DebugClient::~DebugClient(){
	//
}

void DebugClient::UpdateTranslation(){
	xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pbackend->pcon,pbackend->window);
	xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pbackend->pcon,geometryCookie,0);
	if(!pgeometryReply)
		throw("Invalid geometry size - unable to retrieve.");
	glm::uvec2 se(pgeometryReply->width,pgeometryReply->height);
	free(pgeometryReply);

	glm::vec4 screen(se.x,se.y,se.x,se.y);
	glm::vec2 aspect = glm::vec2(1.0,screen.x/screen.y);
	glm::vec4 coord = glm::vec4(pcontainer->p+pcontainer->borderWidth*aspect,pcontainer->e-2.0f*pcontainer->borderWidth*aspect)*screen;
	rect = (WManager::Rectangle){coord.x,coord.y,coord.z,coord.w};

	AdjustSurface1();
}

void DebugClient::Kill(){
	//
}

DebugContainer::DebugContainer(X11Backend *_pbackend) : WManager::Container(), pbackend(_pbackend){
	//
}

DebugContainer::DebugContainer(WManager::Container *_pParent, const WManager::Container::Setup &_setup, X11Backend *_pbackend) : WManager::Container(_pParent,_setup), pbackend(_pbackend){
	//
}

DebugContainer::~DebugContainer(){
	//
}

void DebugContainer::Focus1(){
	//
	printf("focusing ...\n");
}

void DebugContainer::Stack1(){
	//
	printf("stacking ...\n");
}

Debug::Debug() : X11Backend(){
	//
}

Debug::~Debug(){
	//
	xcb_destroy_window(pcon,window);
	xcb_flush(pcon);
}

void Debug::Start(){
	sint scount;
	pcon = xcb_connect(0,&scount);
	if(xcb_connection_has_error(pcon))
		throw Exception("Failed to connect to X server.\n");

	const xcb_setup_t *psetup = xcb_get_setup(pcon);
	xcb_screen_iterator_t sm = xcb_setup_roots_iterator(psetup);
	for(sint i = 0; i < scount; ++i)
		xcb_screen_next(&sm);

	pscr = sm.data;
	if(!pscr)
		throw Exception("Screen unavailable.\n");

	DebugPrintf(stdout,"Screen size: %ux%u\n",pscr->width_in_pixels,pscr->height_in_pixels);

	xcb_key_symbols_t *psymbols = xcb_key_symbols_alloc(pcon);

	exitKeycode = SymbolToKeycode(XK_Q,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1,exitKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	launchKeycode = SymbolToKeycode(XK_space,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_SHIFT,launchKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	closeKeycode = SymbolToKeycode(XK_X,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_SHIFT,closeKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	
	X11KeyBinder binder(psymbols,this);
	DefineBindings(&binder);

	xcb_flush(pcon);

	window = xcb_generate_id(pcon);

	uint values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
	xcb_create_window(pcon,XCB_COPY_FROM_PARENT,window,pscr->root,100,100,800,600,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,pscr->root_visual,XCB_CW_EVENT_MASK,values);
	const char title[] = "chamferwm compositor debug mode";
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,strlen(title),title);
	xcb_map_window(pcon,window);

	xcb_flush(pcon);

	xcb_key_symbols_free(psymbols);
}

sint Debug::HandleEvent(){
	//xcb_generic_event_t *pevent = xcb_poll_for_event(pcon);
	//for(xcb_generic_event_t *pevent = xcb_poll_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
	for(xcb_generic_event_t *pevent = xcb_wait_for_event(pcon); pevent; pevent = xcb_poll_for_event(pcon)){
		//switch(pevent->response_type & ~0x80){
		switch(pevent->response_type & 0x7f){
		/*case XCB_EXPOSE:{
			printf("expose\n");
			}
			break;*/
		case XCB_CLIENT_MESSAGE:{
			DebugPrintf(stdout,"message\n");
			//xcb_client_message_event_t *pev = (xcb_client_message_event_t*)pevent;
			//if(pev->data.data32[0] == wmD
			}
			break;
		case XCB_KEY_PRESS:{
			xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
			if(pev->state & XCB_MOD_MASK_1 && pev->detail == exitKeycode){
				free(pevent);
				return -1;
			}else
			if(pev->detail == launchKeycode){
				//create test client
				DebugClient::CreateInfo createInfo = {};
				createInfo.pbackend = this;
				DebugClient *pclient = SetupClient(&createInfo);
				if(!pclient)
					break;
				clients.push_back(pclient);
			}else
			if(pev->detail == closeKeycode){
				if(clients.size() == 0)
					break;
				DebugClient *pclient = clients.back(); //!!!
				DestroyClient(pclient);
				
				clients.pop_back();
			}else
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,true);
					break;
				}
			}
			}
			break;
		case XCB_KEY_RELEASE:{
			xcb_key_release_event_t *pev = (xcb_key_release_event_t*)pevent;
			for(KeyBinding &binding : keycodes){
				if(pev->state == binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,false);
					break;
				}
			}
			}
			break;
		default:{
			DebugPrintf(stdout,"default event: %u\n",pevent->response_type & 0x7f);
			//TODO: find client here
			//X11Event event11(pevent); //DebugEvent
			//EventNotify(&event11);
			}
			break;
		}

		free(pevent);
		xcb_flush(pcon);
	}

	if(xcb_connection_has_error(pcon)){
		DebugPrintf(stderr,"X server connection lost\n");
		return -1;
	}

	return 1;
}

X11Client * Debug::FindClient(xcb_window_t window, MODE mode) const{
	return 0;
}

};

