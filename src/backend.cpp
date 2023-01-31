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
#include <xcb/xcb_util.h>

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

X11Client::X11Client(X11Container *pcontainer, const CreateInfo *pcreateInfo) : Client(pcontainer), window(pcreateInfo->window), pbackend(pcreateInfo->pbackend), flags(0){
	uint values[1] = {XCB_EVENT_MASK_ENTER_WINDOW|XCB_EVENT_MASK_PROPERTY_CHANGE|XCB_EVENT_MASK_FOCUS_CHANGE};
	xcb_change_window_attributes(pbackend->pcon,window,XCB_CW_EVENT_MASK,values);

	const xcb_atom_t allowedActions[] = {
		pbackend->ewmh._NET_WM_ACTION_CLOSE,
		pbackend->ewmh._NET_WM_ACTION_FULLSCREEN
	};
	xcb_change_property(pbackend->pcon,XCB_PROP_MODE_REPLACE,window,pbackend->ewmh._NET_WM_ALLOWED_ACTIONS,XCB_ATOM_ATOM,32,sizeof(allowedActions)/sizeof(allowedActions[0]),allowedActions);

	if(pcreateInfo->mode == CreateInfo::CREATE_CONTAINED && !(pcontainer->flags & WManager::Container::FLAG_FLOATING)){
		UpdateTranslation();
		oldRect = rect;
	}else{
		rect = *pcreateInfo->prect;
		oldRect = rect;
		translationTime = std::numeric_limits<timespec>::lowest(); //make sure we don't render when nothing is moving

		glm::vec2 screen(pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels);
		pcontainer->p = glm::vec2(rect.x,rect.y)/screen;
		pcontainer->e = glm::vec2(rect.w,rect.h)/screen;
		pcontainer->size = pcontainer->e;
	}

	if(pcreateInfo->mode != CreateInfo::CREATE_AUTOMATIC)
		xcb_map_window(pbackend->pcon,window);
}

X11Client::~X11Client(){
	//
}

void X11Client::UpdateTranslation(){
	glm::vec4 screen(pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels,pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels);
	glm::vec2 aspect = glm::vec2(1.0,screen.x/screen.y);
	glm::vec4 coord = glm::vec4(pcontainer->p,pcontainer->e)*screen;
	if(!(pcontainer->flags & WManager::Container::FLAG_FULLSCREEN || pcontainer->flags & WManager::Container::FLAG_FLOATING))
		coord += glm::vec4(pcontainer->margin*aspect,-2.0f*pcontainer->margin*aspect)*screen;
	
	//WManager::Container *pstackContainer = (pcontainer->pParent->flags & WManager::Container::FLAG_STACKED)?pcontainer->pParent:pcontainer;

	glm::vec2 titlePad = pcontainer->titlePad*aspect*glm::vec2(screen);
	//TODO: check if titlePad is larger than content
	//if(titlePad.x > coord.z)
	glm::vec2 titlePadOffset = glm::min(titlePad,glm::vec2(0.0f));
	glm::vec2 titlePadExtent = glm::max(titlePad,glm::vec2(0.0f));

	if(!(pcontainer->flags & WManager::Container::FLAG_FULLSCREEN) && (!pcontainer->titleStackOnly || (pcontainer->pParent && pcontainer->pParent->flags & WManager::Container::FLAG_STACKED))){
		stackIndex = 0;
		if(pcontainer->pParent->flags & WManager::Container::FLAG_STACKED){
			for(WManager::Container *pcontainer1 = pcontainer->pParent->pch; pcontainer1; ++stackIndex, pcontainer1 = pcontainer1->pnext)
				if(pcontainer1 == pcontainer)
					break;
		
			float stackOffset = 1.0f/(float)std::max((unsigned long)pcontainer->pParent->stackQueue.size(),1lu);
			pcontainer->titleSpan = glm::vec2((float)stackIndex*stackOffset,((float)stackIndex+1.0f)*stackOffset);

			titleFrameExtent = glm::vec2(coord.z,coord.w)*stackOffset;
			titleStackOffset = (float)stackIndex*titleFrameExtent;
		}else{
			pcontainer->titleSpan = glm::vec2(0.0f,1.0f);
			titleFrameExtent = glm::vec2(coord.z,coord.w);
			titleStackOffset = glm::vec2(0.0f);
		}

		coord -= glm::vec4(titlePadOffset.x,titlePadOffset.y,titlePadExtent.x-titlePadOffset.x,titlePadExtent.y-titlePadOffset.y);

		glm::vec2 titleFrameOffset = glm::vec2(coord)+titlePadOffset;
		if(titlePad.x > 1e-3f) //TODO: vectorize
			titleFrameOffset.x += coord.z;
		if(titlePad.y > 1e-3f)
			titleFrameOffset.y += coord.w;
		
		glm::vec2 titlePadAbs = glm::abs(titlePad);
		if(titlePadAbs.x > titlePadAbs.y){
			titleFrameOffset.y += titleStackOffset.y;
			titleFrameExtent.x = titlePadAbs.x;
		}else{
			titleFrameOffset.x += titleStackOffset.x;
			titleFrameExtent.y = titlePadAbs.y;
		}
		
		titleRect = (WManager::Rectangle){titleFrameOffset.x,titleFrameOffset.y,titleFrameExtent.x,titleFrameExtent.y};
		titlePad1 = titlePad;
	}else stackIndex = 0;

	oldRect = rect;
	clock_gettime(CLOCK_MONOTONIC,&translationTime);
	rect = (WManager::Rectangle){coord.x,coord.y,coord.z,coord.w};

	uint values[4] = {rect.x,rect.y,rect.w,rect.h};
	xcb_configure_window(pbackend->pcon,window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,values);

	xcb_flush(pbackend->pcon);

	if(flags & FLAG_UNMAPPING)
		return; //if the window is about to be destroyed, do not attempt to adjust the surface of anything
	
	//Virtual function gets called only if the compositor client class has been initialized.
	//In case compositor client class hasn't been initialized yet, this will be taken care of
	//later on subsequent constructor calls.
	//if(oldRect.w != rect.w || oldRect.h != rect.h) //<- moved to compositor
	AdjustSurface1();
	//surface adjusted on configure_notify
}

void X11Client::UpdateTranslation(const WManager::Rectangle *prect){
	glm::vec2 screen(pbackend->pscr->width_in_pixels,pbackend->pscr->height_in_pixels);
	pcontainer->p = glm::vec2(prect->x,prect->y)/screen;
	pcontainer->e = glm::vec2(prect->w,prect->h)/screen;
	pcontainer->size = pcontainer->e;

	//assumes that the xcb configuration has already been performed - automatic windows do this.
	//alternatively, check if window is manually managed
	oldRect = rect;
	clock_gettime(CLOCK_MONOTONIC,&translationTime);
	rect = *prect;

	AdjustSurface1();
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

X11Container::X11Container(X11Backend *_pbackend, bool _noComp) : WManager::Container(), pbackend(_pbackend), noComp(_noComp){
	//
	uint values[1] = {WManager::Container::rootQueue.size()};
	if(!pbackend->standaloneComp)
		xcb_change_property(pbackend->pcon,XCB_PROP_MODE_REPLACE,pbackend->pscr->root,pbackend->ewmh._NET_NUMBER_OF_DESKTOPS,XCB_ATOM_CARDINAL,32,1,values);
}

X11Container::X11Container(WManager::Container *_pParent, const WManager::Container::Setup &_setup, X11Backend *_pbackend) : WManager::Container(_pParent,_setup), pbackend(_pbackend), noComp(false){
	//
}

X11Container::~X11Container(){
	//
	//_NET_NUMBER_OF_DESKTOPS would be decremented here, but at the moment removal of created workspaces isn't supported
}

void X11Container::Focus1(){
	//during Focus1() the global treeFocus is still the previous focus
	WManager::Container *pPrevRoot = WManager::Container::ptreeFocus->GetRoot();
	WManager::Container *proot = GetRoot();
	if(pPrevRoot != proot){
		printf("Workspace switched [%s] -> [%s].\n",pPrevRoot->pname,proot->pname);
		//recursive unmapping
		X11Backend *pbackend11 = const_cast<X11Backend *>(pbackend);
		X11Container *pPrevRoot11 = static_cast<X11Container *>(pPrevRoot);
		pbackend11->ForEach(pPrevRoot11,[](X11Container *proot1, WManager::Client *pclient)->void{
			X11Client *pclient11 = static_cast<X11Client *>(pclient);
			xcb_unmap_window(pclient11->pbackend->pcon,pclient11->window);

			pclient11->StopComposition1();
		});
		//recursive mapping
		X11Container *proot11 = static_cast<X11Container *>(proot);
		pbackend11->ForEach(proot11,[](X11Container *proot1, WManager::Client *pclient)->void{
			X11Client *pclient11 = static_cast<X11Client *>(pclient);
			xcb_map_window(pclient11->pbackend->pcon,pclient11->window);

			//test if nocomp ws
			//true: unmap overlay, unredirected = true
			//false: check if unredirected == true, map the overlay, unredirected = false
			/*if(strcmp(proot1->pname,"nocomp") == 0){ //todo: flag for nocomp
				pclient11->Unredirect1();
				//TODO: Unredirect1() needs to trigger suspend on compositor
				//unredir
			}*/
			//
			if(!proot1->noComp)
				pclient11->StartComposition1();
		});

		auto m = std::find(WManager::Container::rootQueue.begin(),WManager::Container::rootQueue.end(),proot);
		if(m != WManager::Container::rootQueue.end()){
			uint values[1] = {m-WManager::Container::rootQueue.begin()};
			xcb_change_property(pbackend->pcon,XCB_PROP_MODE_REPLACE,pbackend->pscr->root,pbackend->ewmh._NET_CURRENT_DESKTOP,XCB_ATOM_CARDINAL,32,1,values);
		}
	}
	const xcb_window_t *pfocusWindow;
	if(pclient){
		pfocusWindow = &pclient11->window;

		//TODO: how to handle Container::FLAG_NO_FOCUS?
		//if(ProtocolSupport(pbackend->atoms[X11Backend::ATOM_WM_TAKE_FOCUS]))
			//send take focus

	}else pfocusWindow = &pbackend->ewmh_window;

	xcb_set_input_focus(pbackend->pcon,XCB_INPUT_FOCUS_POINTER_ROOT,*pfocusWindow,pbackend->lastTime);
	xcb_change_property(pbackend->pcon,XCB_PROP_MODE_REPLACE,pbackend->pscr->root,pbackend->ewmh._NET_ACTIVE_WINDOW,XCB_ATOM_WINDOW,32,1,pfocusWindow);

	xcb_flush(pbackend->pcon);
}

void X11Container::Place1(WManager::Container *pOrigParent){
	WManager::Container *proot = WManager::Container::ptreeFocus->GetRoot();
	if(pOrigParent->GetRoot() != proot){
		if(GetRoot() == proot){
			xcb_map_window(pclient11->pbackend->pcon,pclient11->window);

			pclient11->StartComposition1();

		}
	}else{
		if(GetRoot() != proot){
			xcb_unmap_window(pclient11->pbackend->pcon,pclient11->window);

			pclient11->StopComposition1();
		}
	}
}

void X11Container::Stack1(){
	if(!pbackend->standaloneComp)
		const_cast<X11Backend *>(pbackend)->StackClients(GetRoot());
}

void X11Container::Fullscreen1(){
	if(!pclient)
		return;
	if(flags & FLAG_FULLSCREEN){
		xcb_change_property(pbackend->pcon,XCB_PROP_MODE_APPEND,pclient11->window,pbackend->ewmh._NET_WM_STATE,XCB_ATOM_ATOM,32,1,&pbackend->ewmh._NET_WM_STATE_FULLSCREEN);
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
	}

	xcb_flush(pbackend->pcon);
}

void X11Container::SetClient(X11Client *_pclient11){
	pclient11 = _pclient11;
	pclient = pclient11;
}

X11Backend::X11Backend(bool _standaloneComp) : lastTime(XCB_CURRENT_TIME), standaloneComp(_standaloneComp){
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

void X11Backend::StackRecursiveAppendix(const WManager::Client *pclient, const WManager::Container *pfocus){
	auto s = [&](auto &p)->bool{
		return pclient == p.first;
	};
	//TODO: recursively check that none of the base clients will be untransient, and thus placed wrongly on top of everything
	for(auto m = std::find_if(appendixQueue.begin(),appendixQueue.end(),s);
		m != appendixQueue.end(); m = std::find_if(m,appendixQueue.end(),s)){
		X11Client *pclient11 = static_cast<X11Client *>((*m).second);

		if(pclient11){
			static uint values[1] = {XCB_STACK_MODE_ABOVE};
			xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
		}

		clientStack.push_back((*m).second);

		StackRecursiveAppendix((*m).second,pfocus);

		m = appendixQueue.erase(m);
	}
}

void X11Backend::StackRecursive(const WManager::Container *pcontainer, const WManager::Container *pfocus){
	for(WManager::Container *pcont : pcontainer->stackQueue){
		if(pcont->pclient){
			X11Client *pclient11 = static_cast<X11Client *>(pcont->pclient);

			if(pclient11){
				static uint values[1] = {XCB_STACK_MODE_ABOVE};
				xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
			}

			clientStack.push_back(pcont->pclient);
		}
		StackRecursive(pcont,pcontainer == pfocus?pcont:pfocus);
	}

	if(!pcontainer->pclient)
		return;
	StackRecursiveAppendix(pcontainer->pclient,pfocus);
}

void X11Backend::StackClients(const WManager::Container *proot){
	SortStackAppendix();

	const std::vector<std::pair<const WManager::Client *, WManager::Client *>> *pstackAppendix = GetStackAppendix();

	appendixQueue.clear();
	clientStack.clear();

	for(auto &p : *pstackAppendix){
		if(!p.second)
			continue;
		if(p.first){
			appendixQueue.push_back(p);
			continue;
		}

		X11Client *pclient11 = static_cast<X11Client *>(p.second);

		if(pclient11){
			static uint values[1] = {XCB_STACK_MODE_ABOVE};
			xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
		}
		
		//desktop features are placed first
		clientStack.push_back(p.second);
	}

	StackRecursive(proot,WManager::Container::ptreeFocus);

	for(auto m = appendixQueue.begin(); m != appendixQueue.end();){
		if(!(*m).second)
			continue;
		auto k = std::find_if(appendixQueue.begin(),appendixQueue.end(),[&](auto &p)->bool{
			return (*m).first == p.second;
		});
		if(k != appendixQueue.end()){
			++m;
			continue;
		}
			
		X11Client *pclient11 = static_cast<X11Client *>((*m).second);

		if(pclient11){
			static uint values[1] = {XCB_STACK_MODE_ABOVE};
			xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values);
		}

		clientStack.push_back((*m).second);

		StackRecursiveAppendix((*m).second,WManager::Container::ptreeFocus);

		m = appendixQueue.erase(m);
	}

	for(auto &p : appendixQueue){ //stack the remaining (untransient) windows
		if(!p.second)
			continue;
		X11Client *pclient11 = static_cast<X11Client *>(p.second);

		if(pclient11){
			static uint values[1] = {XCB_STACK_MODE_ABOVE};
			xcb_configure_window(pcon,pclient11->window,XCB_CONFIG_WINDOW_STACK_MODE,values); //crashes often
		}

		clientStack.push_back(p.second);
	}
}

void X11Backend::ForEachRecursive(X11Container *proot, WManager::Container *pcontainer, void (*pf)(X11Container *, WManager::Client *)){
	for(WManager::Container *pcont : pcontainer->stackQueue){
		if(pcont->pclient)
			pf(proot,pcont->pclient);
		ForEachRecursive(proot,pcont,pf);
	}
}

void X11Backend::ForEach(X11Container *proot, void (*pf)(X11Container *, WManager::Client *)){
	ForEachRecursive(proot,proot,pf);

	const std::vector<std::pair<const WManager::Client *, WManager::Client *>> *pstackAppendix = GetStackAppendix();
	for(auto &p : *pstackAppendix)
		if(p.second->pcontainer->GetRoot() == proot)
			pf(proot,p.second);
}

void X11Backend::BindKey(uint symbol, uint mask, uint keyId){
	xcb_keycode_t keycode = SymbolToKeycode(symbol,psymbols);
	xcb_grab_key(pcon,1,pscr->root,mask,keycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	X11Backend::KeyBinding binding;
	binding.keycode = keycode;
	binding.mask = mask;
	binding.keyId = keyId;
	keycodes.push_back(binding);
}

void X11Backend::MapKey(uint symbol, uint mask, uint keyId){
	X11Backend::KeyBinding binding;
	binding.keycode = SymbolToKeycode(symbol,psymbols);
	binding.mask = mask;
	binding.keyId = keyId;
	//printf("map %x, %x, %u\n",mask,binding.keycode,keyId);
	keycodes.push_back(binding);
}

void X11Backend::GrabKeyboard(bool enable){
	if(enable)
		xcb_grab_keyboard(pcon,0,pscr->root,lastTime,XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	else xcb_ungrab_keyboard(pcon,lastTime);
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
	"WM_PROTOCOLS","WM_DELETE_WINDOW","WM_TAKE_FOCUS","ESETROOT_PMAP_ID","_X_ROOTPMAP_ID"
};

const char *X11Backend::pcursorStrs[CURSOR_COUNT] = {
	"left_ptr"
};

Default::Default(bool _standaloneComp) : X11Backend(_standaloneComp), pdragClient(0){
	//
	clock_gettime(CLOCK_MONOTONIC,&eventTimer);
	pollTimer.tv_sec = 0;
	pollTimer.tv_nsec = 0;
	//polling = false;
	
	inputTimer = eventTimer;
}

Default::~Default(){
	//sigprocmask(SIG_UNBLOCK,&signals,0);
}

void Default::Start(){
	sint scount;
	pcon = xcb_connect(0,&scount);
	if(xcb_connection_has_error(pcon))
		throw Exception("Failed to connect to X server.");

	const xcb_setup_t *psetup = xcb_get_setup(pcon);
	xcb_screen_iterator_t sm = xcb_setup_roots_iterator(psetup);
	for(sint i = 0; i < scount; ++i)
		xcb_screen_next(&sm);

	pscr = sm.data;
	if(!pscr)
		throw Exception("Screen unavailable.");

	DebugPrintf(stdout,"Screen size: %ux%u (DPI: %fx%f)\n",pscr->width_in_pixels,pscr->height_in_pixels,25.4f*(float)pscr->width_in_pixels/(float)pscr->width_in_millimeters,0.0f);
	//https://standards.freedesktop.org/wm-spec/wm-spec-1.3.html#idm140130317705584

	psymbols = xcb_key_symbols_alloc(pcon);

	exitKeycode = SymbolToKeycode(XK_E,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1|XCB_MOD_MASK_SHIFT,exitKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	
	if(!standaloneComp)
		DefineBindings();

	xcb_flush(pcon);

	uint values[4] = {(!standaloneComp?XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT:0)
		|XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
		|XCB_EVENT_MASK_EXPOSURE
		|XCB_EVENT_MASK_PROPERTY_CHANGE,0,0,0}; //[4] for later use
	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(pcon,pscr->root,XCB_CW_EVENT_MASK,values);
	xcb_generic_error_t *perr = xcb_request_check(pcon,cookie);

	xcb_flush(pcon);

	window = pscr->root;
	DebugPrintf(stdout,"Root id: %x\n",window);

	if(perr != 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Substructure redirection failed (%d). WM already present.\n",perr->error_code);
		xcb_disconnect(pcon);
		throw Exception();
	}

	xcb_intern_atom_cookie_t *patomCookie = xcb_ewmh_init_atoms(pcon,&ewmh);
	if(!xcb_ewmh_init_atoms_replies(&ewmh,patomCookie,0)){
		xcb_disconnect(pcon);
		throw Exception("Failed to initialize EWMH atoms.\n");
	}
	
	for(uint i = 0; i < ATOM_COUNT; ++i)
		atoms[i] = GetAtom(patomStrs[i]);
	
	//setup EWMH hints
	ewmh_window = xcb_generate_id(pcon);

	values[0] = 1;
	xcb_create_window(pcon,XCB_COPY_FROM_PARENT,ewmh_window,pscr->root,
		-1,-1,1,1,0,XCB_WINDOW_CLASS_INPUT_ONLY,XCB_COPY_FROM_PARENT,XCB_CW_OVERRIDE_REDIRECT,values);
	
	values[0] = XCB_STACK_MODE_BELOW;
	xcb_configure_window(pcon,ewmh_window,XCB_CONFIG_WINDOW_STACK_MODE,values);
	xcb_map_window(pcon,ewmh_window);

	if(standaloneComp){
		DebugPrintf(stdout,"Launching in standalone compositor mode.\n");
		//send an internal map notification to let the compositor know about the existing windows
		xcb_query_tree_cookie_t queryTreeCookie = xcb_query_tree(pcon,pscr->root);
		xcb_query_tree_reply_t *pqueryTreeReply = xcb_query_tree_reply(pcon,queryTreeCookie,0);
		xcb_window_t *pchs = xcb_query_tree_children(pqueryTreeReply);
		for(uint i = 0, n = xcb_query_tree_children_length(pqueryTreeReply); i < n; ++i){
			char buffer[32];

			xcb_map_notify_event_t *pev = (xcb_map_notify_event_t*)buffer;
			pev->event = pchs[i];
			pev->window = pchs[i];
			pev->response_type = XCB_MAP_NOTIFY;
			pev->override_redirect = true;

			xcb_send_event(pcon,false,ewmh_window,XCB_EVENT_MASK_NO_EVENT,buffer);
		}
		free(pqueryTreeReply);

		xcb_flush(pcon);

	}else{
		const char wmName[] = "chamfer";

		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,ewmh_window,ewmh._NET_SUPPORTING_WM_CHECK,XCB_ATOM_WINDOW,32,1,&ewmh_window);
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,ewmh_window,ewmh._NET_WM_NAME,XCB_ATOM_STRING,8,strlen(wmName),wmName);

		//https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html#idm140130317705584
		const xcb_atom_t supportedAtoms[] = {
			ewmh._NET_WM_NAME,
			ewmh._NET_CLIENT_LIST,
			//TODO: _NET_CLIENT_LIST_STACKING
			ewmh._NET_CURRENT_DESKTOP,
			ewmh._NET_NUMBER_OF_DESKTOPS,
			//ewmh._NET_NUMBER_OF_DESKTOPS
			ewmh._NET_DESKTOP_VIEWPORT,
			ewmh._NET_DESKTOP_GEOMETRY,
			ewmh._NET_ACTIVE_WINDOW,
			ewmh._NET_WORKAREA,
			ewmh._NET_WM_STATE,
			ewmh._NET_WM_STATE_FULLSCREEN,
			ewmh._NET_WM_STATE_DEMANDS_ATTENTION,
			ewmh._NET_ACTIVE_WINDOW,
			ewmh._NET_CLOSE_WINDOW,
			//ewmh._NET_WM_DESKTOP,
			ewmh._NET_MOVERESIZE_WINDOW
		};

		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_SUPPORTING_WM_CHECK,XCB_ATOM_WINDOW,32,1,&ewmh_window);
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_SUPPORTED,XCB_ATOM_ATOM,32,sizeof(supportedAtoms)/sizeof(supportedAtoms[0]),supportedAtoms);

		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_WM_NAME,XCB_ATOM_STRING,8,strlen(wmName),wmName);
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,0,0);
		values[0] = 0;
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CURRENT_DESKTOP,XCB_ATOM_CARDINAL,32,1,values);
		values[0] = 1;
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_NUMBER_OF_DESKTOPS,XCB_ATOM_CARDINAL,32,1,values);
		//xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_DESKTOP_NAMES,XCB_ATOM_STRING,8,0,0);
		values[0] = 0; //viewport x
		values[1] = 0; //viewport y
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_DESKTOP_VIEWPORT,XCB_ATOM_CARDINAL,32,2,values);
		values[0] = pscr->width_in_pixels; //geometry x
		values[1] = pscr->height_in_pixels; //geometry y
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_DESKTOP_GEOMETRY,XCB_ATOM_CARDINAL,32,2,values);
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_ACTIVE_WINDOW,XCB_ATOM_WINDOW,32,1,&ewmh_window);
		values[0] = 0;
		values[1] = 0;
		values[2] = pscr->width_in_pixels;
		values[3] = pscr->height_in_pixels;
		xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_WORKAREA,XCB_ATOM_CARDINAL,32,4,values);
	
		pcursorctx = 0;
		if(xcb_cursor_context_new(pcon,pscr,&pcursorctx) >= 0){
			for(uint i = 0; i < CURSOR_COUNT; ++i)
				cursors[i] = xcb_cursor_load_cursor(pcursorctx,pcursorStrs[i]);
		
			xcb_change_window_attributes(pcon,pscr->root,XCB_CW_CURSOR,&cursors[CURSOR_POINTER]);
		}
	}
}

void Default::Stop(){
	//cleanup
	if(!standaloneComp){
		if(pcursorctx)
			xcb_cursor_context_free(pcursorctx);
		xcb_set_input_focus(pcon,XCB_NONE,XCB_INPUT_FOCUS_POINTER_ROOT,XCB_CURRENT_TIME);
	}
	
	xcb_destroy_window(pcon,ewmh_window);
	xcb_ewmh_connection_wipe(&ewmh);

	xcb_key_symbols_free(psymbols);

	xcb_disconnect(pcon);
	xcb_flush(pcon);
}

sint Default::HandleEvent(bool forcePoll){
	struct timespec currentTime;
	clock_gettime(CLOCK_MONOTONIC,&currentTime);
	
	/*if(timespec_diff(currentTime,pollTimer) >= 0.04){
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
		if(timespec_diff(currentTime,eventTimer) >= 5.0){
			TimerEvent();
			clock_gettime(CLOCK_MONOTONIC,&eventTimer);
		}
	}*/

	//polling = false;

	sint result = 0;

	for(xcb_generic_event_t *pevent = forcePoll?xcb_poll_for_event(pcon):xcb_wait_for_event(pcon);
		pevent; pevent = xcb_poll_for_event(pcon)){
		//Event found, move to polling mode for some time.
		clock_gettime(CLOCK_MONOTONIC,&pollTimer);
		//polling = true;

		lastTime = XCB_CURRENT_TIME;

		//switch(pevent->response_type & ~0x80){
		switch(pevent->response_type & 0x7f){
		case XCB_CREATE_NOTIFY:{
			xcb_create_notify_event_t *pev = (xcb_create_notify_event_t*)pevent;
			
			WManager::Rectangle rect = {pev->x,pev->y,pev->width,pev->height};
			
			if(!configCache.empty())
				std::get<2>(configCache.back()) = pev->window;

			auto mrect = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == std::get<0>(p);//p.first;
			});
			if(mrect == configCache.end()){
				configCache.push_back(ConfigCacheElement(pev->window,rect,0));
				mrect = configCache.end()-1;
			}else{ //tbh there should not be any
				std::get<1>(*mrect) = rect;
				std::get<2>(*mrect) = 0;
			}
			//debug: print current and expected stacking order (standalone comp).
			//-----------------------------------------
#if 0
#define debug_query_tree(m) if(standaloneComp){\
			printf("%s\n",m);\
			xcb_query_tree_cookie_t queryTreeCookie = xcb_query_tree(pcon,pscr->root);\
			xcb_query_tree_reply_t *pqueryTreeReply = xcb_query_tree_reply(pcon,queryTreeCookie,0);\
			xcb_window_t *pchs = xcb_query_tree_children(pqueryTreeReply);\
			printf("E\tC\n");\
			for(uint i = 0, n = xcb_query_tree_children_length(pqueryTreeReply); i < n || i < configCache.size(); ++i){\
				printf("%x\t%x\t->%x\n",i < n?pchs[i]:0xff,i < configCache.size()?std::get<0>(configCache[i]):0xff,\
					i < configCache.size()?std::get<2>(configCache[i]):0xff);\
			}\
			free(pqueryTreeReply);}
#else
#define debug_query_tree(m) {}
#endif
			//-----------------------------------------
			debug_query_tree("CREATE");

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
				return pev->window == std::get<0>(p);
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
			/*if(!allowPositionConfig){
				if((rect.x == 0 && pev->value_mask & XCB_CONFIG_WINDOW_X) || (mrect != configCache.end() && (*mrect).second.x == 0 && !(pev->value_mask & XCB_CONFIG_WINDOW_X))){ //TODO: need this same check in MAP_REQUEST
					rect.x = (pscr->width_in_pixels-rect.w)/2;
					pev->value_mask |= XCB_CONFIG_WINDOW_X;
				}
				if((rect.y == 0 && pev->value_mask & XCB_CONFIG_WINDOW_Y) || (mrect != configCache.end() && (*mrect).second.y == 0 && !(pev->value_mask & XCB_CONFIG_WINDOW_Y))){
					rect.y = (pscr->height_in_pixels-rect.h)/2;
					pev->value_mask |= XCB_CONFIG_WINDOW_Y;
				}
			}*/
			
			if(mrect == configCache.end()){
				//an entry should always be present, since CREATE_NOTIFY creates one
				configCache.push_back(ConfigCacheElement(pev->window,rect,0));
				mrect = configCache.end()-1;
			}else{
				if(pev->value_mask & XCB_CONFIG_WINDOW_X)
					std::get<1>(*mrect).x = rect.x;
				if(pev->value_mask & XCB_CONFIG_WINDOW_Y)
					std::get<1>(*mrect).y = rect.y;
				if(pev->value_mask & XCB_CONFIG_WINDOW_WIDTH)
					std::get<1>(*mrect).w = rect.w;
				if(pev->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
					std::get<1>(*mrect).h = rect.h;
			}

			struct{
				uint16_t m;
				uint32_t v;
			} maskm[] = {
				{XCB_CONFIG_WINDOW_X,std::get<1>(*mrect).x},
				{XCB_CONFIG_WINDOW_Y,std::get<1>(*mrect).y},
				{XCB_CONFIG_WINDOW_WIDTH,std::get<1>(*mrect).w},
				{XCB_CONFIG_WINDOW_HEIGHT,std::get<1>(*mrect).h},
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
				pclient1->UpdateTranslation(&std::get<1>(*mrect));

			DebugPrintf(stdout,"configure request: %x | %d, %d, %u, %u (mask: %x) -> %d, %d, %u, %u\n",pev->window,pev->x,pev->y,pev->width,pev->height,pev->value_mask,std::get<1>(*mrect).x,std::get<1>(*mrect).y,std::get<1>(*mrect).w,std::get<1>(*mrect).h);
			}
			break;
		case XCB_MAP_REQUEST:{
			xcb_map_request_event_t *pev = (xcb_map_request_event_t*)pevent;
			if(pev->window == ewmh_window){
				xcb_map_window(pcon,pev->window);
				break;
			}

			DebugPrintf(stdout,"map request, %x\n",pev->window);

			result = 1;
			
			//check if window already exists
			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(pclient1){
				xcb_map_window(pcon,pev->window);
				break;
			}

			X11Client *pbaseClient = 0;
			uint hintFlags = 0;

			xcb_get_property_cookie_t propertyCookieHints = xcb_icccm_get_wm_hints(pcon,pev->window);
			xcb_get_property_cookie_t propertyCookieNormalHints = xcb_icccm_get_wm_normal_hints(pcon,pev->window);
			xcb_get_property_cookie_t propertyCookieWindowType
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_WINDOW_TYPE,XCB_ATOM_ATOM,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookieWindowState
				= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_STATE,XCB_ATOM_ATOM,0,std::numeric_limits<uint32_t>::max());
			//xcb_get_property_cookie_t propertyCookieStrut
				//= xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_STRUT_PARTIAL,XCB_ATOM_CARDINAL,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookieTransientFor
				= xcb_get_property(pcon,0,pev->window,XA_WM_TRANSIENT_FOR,XCB_ATOM_WINDOW,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookie1[2];
			propertyCookie1[0] = xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_NAME,XCB_GET_PROPERTY_TYPE_ANY,0,128);
			propertyCookie1[1] = xcb_get_property(pcon,0,pev->window,XCB_ATOM_WM_CLASS,XCB_GET_PROPERTY_TYPE_ANY,0,128);

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
				return pev->window == std::get<0>(p);
			});
			if(mrect == configCache.end()){
				//an entry should always be present, since CREATE_NOTIFY creates one
				WManager::Rectangle rect;
				configCache.push_back(ConfigCacheElement(pev->window,rect,0));
				mrect = configCache.end()-1;
			}
			//printf("Found cached location: %d, %d, %u, %u.\n",(*mrect).second.x,(*mrect).second.y,(*mrect).second.w,(*mrect).second.h);

			if(boolHints){
				if(hints.flags & XCB_ICCCM_WM_HINT_INPUT && !hints.input)
					hintFlags |= X11Client::CreateInfo::HINT_NO_INPUT;
			}
			if(boolSizeHints){
				if(sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE &&
				sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE &&
				(sizeHints.min_width == sizeHints.max_width ||
				sizeHints.min_height == sizeHints.max_height)){
					//
					std::get<1>(*mrect).w = sizeHints.min_width;
					std::get<1>(*mrect).h = sizeHints.min_height;
					hintFlags |= X11Client::CreateInfo::HINT_FLOATING;
				}else
				if(sizeHints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE || sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE){
					std::get<1>(*mrect).w = sizeHints.width;
					std::get<1>(*mrect).h = sizeHints.height;
				}

				std::get<1>(*mrect).x = (pscr->width_in_pixels-std::get<1>(*mrect).w)/2;
				std::get<1>(*mrect).y = (pscr->height_in_pixels-std::get<1>(*mrect).h)/2;
				if((sizeHints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION || sizeHints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION) && allowPositionConfig && (sizeHints.x != 0 && sizeHints.y != 0)){
					std::get<1>(*mrect).x = sizeHints.x;
					std::get<1>(*mrect).y = sizeHints.y;
				}
				uint values[4] = {std::get<1>(*mrect).x,std::get<1>(*mrect).y,std::get<1>(*mrect).w,std::get<1>(*mrect).h};
				xcb_configure_window(pcon,pev->window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,values);
			}
			printf("Size hints: %d, %d, %u, %u.\n",std::get<1>(*mrect).x,std::get<1>(*mrect).y,std::get<1>(*mrect).w,std::get<1>(*mrect).h);

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

						hintFlags |= X11Client::CreateInfo::HINT_FLOATING|
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
						//TODO: BELOW should be above desktop
						//https://specifications.freedesktop.org/wm-spec/1.3/ar01s07.html#STACKINGORDER
						hintFlags |= X11Client::CreateInfo::HINT_FLOATING|
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

			xcb_get_property_reply_t *propertyReply1[2];
			char *pwmProperty[2];

			for(uint i = 0; i < 2; ++i){
				propertyReply1[i] = xcb_get_property_reply(pcon,propertyCookie1[i],0);
				if(!propertyReply1[i]){
					pwmProperty[i] = 0;
					continue;
				}
				uint propertyLen = xcb_get_property_value_length(propertyReply1[i]);
				if(propertyLen <= 0){
					pwmProperty[i] = 0;
					continue;
				}
				pwmProperty[i] = mstrndup((const char *)xcb_get_property_value(propertyReply1[i]),propertyLen);
				pwmProperty[i][propertyLen] = 0;
			}

			BackendStringProperty wmName(pwmProperty[0]?pwmProperty[0]:"");
			BackendStringProperty wmClass(pwmProperty[1]?pwmProperty[1]:"");

			static WManager::Client dummyClient(0); //base client being unavailable means that the client is stacked on top of everything else

			X11Client::CreateInfo createInfo;
			createInfo.window = pev->window;
			createInfo.prect = &std::get<1>(*mrect);
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

			if(pclient->pcontainer->titleBar != WManager::Container::TITLEBAR_NONE && strlen(wmName.pstr) > 0)
				pclient->SetTitle1(wmName.pstr);

			const WManager::Container *proot = pclient->pcontainer->GetRoot();
			StackClients(proot);

			//if(hintFlags & X11Client::CreateInfo::HINT_FULLSCREEN)
				//SetFullscreen(pclient,true);

			for(uint i = 0; i < 2; ++i){
				if(propertyReply1[i])
					free(propertyReply1[i]);
				if(pwmProperty[i])
					mstrfree(pwmProperty[i]);
			}

			netClientList.clear();
			for(auto &p : clients)
				netClientList.push_back(p.first->window);
			xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,netClientList.size(),netClientList.data());

			if(!standaloneComp)
				xcb_grab_button(pcon,0,pev->window,XCB_EVENT_MASK_BUTTON_PRESS,
					XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC,pscr->root,XCB_NONE,1,XCB_MOD_MASK_1);
			//check fullscreen
		
			}
			break;
		case XCB_CONFIGURE_NOTIFY:{
			xcb_configure_notify_event_t *pev = (xcb_configure_notify_event_t*)pevent;

			WManager::Rectangle rect = {pev->x,pev->y,pev->width,pev->height};
			
			//TODO: this and in the CREATE -> function?
			auto mrect = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == std::get<0>(p); //fix the above_sibling reference at the old list location
			});
			if(standaloneComp){
				//order matters when running on other window managers
				//TODO: rotate
				if(mrect != configCache.end()){
					auto m = configCache.erase(mrect);
					if(m != configCache.begin())
						std::get<2>(*(m-1)) = (m != configCache.end())?std::get<0>(*m):0;
				}
				auto mabv = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
					return std::get<0>(p) == pev->above_sibling;
				});
				if(mabv != configCache.end()){
					mrect = configCache.insert(mabv+1,ConfigCacheElement(pev->window,rect,std::get<2>(*mabv))); //insert to new location
					std::get<2>(*mabv) = pev->window; //fix the above_sibling reference at the new list location
				}else{
					//nothing above this client
					configCache.push_back(ConfigCacheElement(pev->window,rect,pev->above_sibling));
					mrect = configCache.end()-1;
				}
			}else{
				if(mrect == configCache.end()){
					configCache.push_back(ConfigCacheElement(pev->window,rect,pev->above_sibling));
					mrect = configCache.end()-1;
				}else{
					std::get<1>(*mrect) = rect;
					std::get<2>(*mrect) = pev->above_sibling;
				}
			}
			debug_query_tree("CONFIG");

			X11Client *pclient1 = FindClient(pev->window,MODE_AUTOMATIC);

			if(pclient1){
				pclient1->UpdateTranslation(&std::get<1>(*mrect));

				//handle stacking order
				if(standaloneComp){
					clientStack.erase(std::remove(clientStack.begin(),clientStack.end(),pclient1),clientStack.end());
					bool found = false;

					//find the first client ("above sibling") that is actually mapped
					for(auto k = std::make_reverse_iterator(mrect); k != configCache.rend(); ++k){ //start from above sibling, go reverse
						//TODO: skip mrect
						auto m = std::find_if(clientStack.begin(),clientStack.end(),[&](auto &p)->bool{ //todo: rbegin/rend()
							return static_cast<X11Client *>(p)->window == std::get<2>(*k);
							//return pev->above_sibling == std::get<0>(*k);
						});
						if(m != clientStack.end()){
							clientStack.insert(m+1,pclient1);
							found = true;
							break;
						}
					}

					if(!found) //nothing was mapped
						clientStack.push_back(pclient1);

					result = 1;
				}
			}

			DebugPrintf(stdout,"configure to %d,%d %ux%u, %x\n",pev->x,pev->y,pev->width,pev->height,pev->window);
			}
			break;
		case XCB_MAP_NOTIFY:{
			xcb_map_notify_event_t *pev = (xcb_map_notify_event_t*)pevent;
			if(pev->window == ewmh_window && !standaloneComp)
				break;

			result = 1;

			//check if window already exists (already mapped)
			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(pclient1){
				//TODO: check if transient_for is in different root this time
				pclient1->flags &= ~X11Client::FLAG_UNMAPPING;
				DebugPrintf(stdout,"Unmapping flag erased.\n");
				break;
			}

			X11Client *pbaseClient = 0;

			xcb_get_property_cookie_t propertyCookieTransientFor
				= xcb_get_property(pcon,0,pev->window,XA_WM_TRANSIENT_FOR,XCB_ATOM_WINDOW,0,std::numeric_limits<uint32_t>::max());
			xcb_get_property_cookie_t propertyCookie1[2];
			propertyCookie1[0] = xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_NAME,XCB_GET_PROPERTY_TYPE_ANY,0,128);
			propertyCookie1[1] = xcb_get_property(pcon,0,pev->window,XCB_ATOM_WM_CLASS,XCB_GET_PROPERTY_TYPE_ANY,0,128);

			xcb_get_property_reply_t *propertyReplyTransientFor
				= xcb_get_property_reply(pcon,propertyCookieTransientFor,0);

			if(propertyReplyTransientFor){
				xcb_window_t *pbaseWindow = (xcb_window_t*)xcb_get_property_value(propertyReplyTransientFor);
				pbaseClient = FindClient(*pbaseWindow,MODE_UNDEFINED);
				free(propertyReplyTransientFor);
			}

			auto mrect = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return pev->window == std::get<0>(p);
			});
			if(mrect == configCache.end()){
				//it might be the case that no configure notification was received
				xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pcon,pev->window);
				xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pcon,geometryCookie,0);
				if(!pgeometryReply)
					break; //hack: happens sometimes on high rate of events
				WManager::Rectangle rect = {pgeometryReply->x,pgeometryReply->y,pgeometryReply->width,pgeometryReply->height};
				free(pgeometryReply);

				if(!configCache.empty()) //ensure that the above_sibling is correctly set during the initial tree query
					std::get<2>(configCache.back()) = pev->window;

				configCache.push_back(ConfigCacheElement(pev->window,rect,0));
				mrect = configCache.end()-1;

			}
			debug_query_tree("MAP");

			WManager::Rectangle *prect = &std::get<1>(*mrect);

			static WManager::Client dummyClient(0);

			const WManager::Client *pstackClient = &dummyClient;
			if(pbaseClient && !standaloneComp){
				auto k = std::find_if(clients.begin(),clients.end(),[&](auto &p)->bool{
					return p.first->window == pbaseClient->window;
				});
				//if the transient window was manually created, place the new automatic window on top of everything
				//printf("transient for %x\n",pbaseClient->window);
				if(k != clients.end() && (*k).second == MODE_AUTOMATIC)
					pstackClient = pbaseClient;
			}

			xcb_get_property_reply_t *propertyReply1[2];
			char *pwmProperty[2];

			for(uint i = 0; i < 2; ++i){
				propertyReply1[i] = xcb_get_property_reply(pcon,propertyCookie1[i],0);
				if(!propertyReply1[i]){
					pwmProperty[i] = 0;
					continue;
				}
				uint propertyLen = xcb_get_property_value_length(propertyReply1[i]);
				if(propertyLen <= 0){
					pwmProperty[i] = 0;
					continue;
				}
				pwmProperty[i] = mstrndup((const char *)xcb_get_property_value(propertyReply1[i]),propertyLen);
				pwmProperty[i][propertyLen] = 0;
			}

			BackendStringProperty wmName(pwmProperty[0]?pwmProperty[0]:"");
			BackendStringProperty wmClass(pwmProperty[1]?pwmProperty[1]:"");

			X11Client::CreateInfo createInfo;
			createInfo.window = pev->window;
			createInfo.prect = prect;
			createInfo.pstackClient = pstackClient;
			createInfo.pbackend = this;
			createInfo.mode = X11Client::CreateInfo::CREATE_AUTOMATIC;
			createInfo.hints = 0;
			createInfo.pwmName = &wmName;
			createInfo.pwmClass = &wmClass;
			X11Client *pclient = SetupClient(&createInfo);
			if(pclient){
				clients.push_back(std::pair<X11Client *, MODE>(pclient,MODE_AUTOMATIC));

				if(!standaloneComp){
					const WManager::Container *proot = pclient->pcontainer->GetRoot();
					StackClients(proot);

					netClientList.clear();
					for(auto &p : clients)
						netClientList.push_back(p.first->window);
					xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,netClientList.size(),netClientList.data());
				}else
				if(ApproveExternal(&wmName,&wmClass)){
					//clientStack won't get cleared in standalone compositor mode
					bool found = false;

					//find the first client ("above sibling") that is actually mapped
					for(auto k = std::make_reverse_iterator(mrect); k != configCache.rend(); ++k){ //start from above sibling, go reverse
						//TODO: skip mrect
						auto m = std::find_if(clientStack.begin(),clientStack.end(),[&](auto &p)->bool{ //todo: rbegin/rend()
							return static_cast<X11Client *>(p)->window == std::get<2>(*k);
							//return std::get<2>(*mrect) == std::get<0>(*k);
						});
						if(m != clientStack.end()){
							clientStack.insert(m+1,pclient);
							found = true;
							break;
						}
					}

					if(!found) //nothing was mapped
						clientStack.push_back(pclient);
				}
			}

			DebugPrintf(stdout,"map notify %x, client: %p \"%s\", \"%s\"\n",pev->window,pclient,wmClass.pstr,wmName.pstr);

			for(uint i = 0; i < 2; ++i){
				if(propertyReply1[i])
					free(propertyReply1[i]);
				if(pwmProperty[i])
					mstrfree(pwmProperty[i]);
			}

			}
			break;
		case XCB_UNMAP_NOTIFY:{
			xcb_unmap_notify_event_t *pev = (xcb_unmap_notify_event_t*)pevent;

			DebugPrintf(stdout,"unmap notify %x",pev->window);

			debug_query_tree("UNMAP");

			auto m = std::find_if(clients.begin(),clients.end(),[&](auto &p)->bool{
				return p.first->window == pev->window;
			});
			if(m == clients.end())
				break;

			result = 1;

			//DebugPrintf(stdout,"unmap notify %x, client: %p\n",pev->window,(*m).first);

			(*m).first->flags |= X11Client::FLAG_UNMAPPING;
			clientStack.erase(std::remove(clientStack.begin(),clientStack.end(),(*m).first),clientStack.end());

			if((*m).first->pcontainer->GetRoot() != WManager::Container::ptreeFocus->GetRoot())
				break;
			
			unmappingQueue.insert((*m).first);

			netClientList.clear();
			for(auto &p : clients)
				netClientList.push_back(p.first->window);
			xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,pscr->root,ewmh._NET_CLIENT_LIST,XCB_ATOM_WINDOW,32,netClientList.size(),netClientList.data());

			std::iter_swap(m,clients.end()-1);
			clients.pop_back();
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
			if(!pclient1 || pclient1->flags & X11Client::FLAG_UNMAPPING)
				break;

			if(pev->atom == XCB_ATOM_WM_NAME){
				xcb_get_property_cookie_t propertyCookie = xcb_get_property(pcon,0,pev->window,ewmh._NET_WM_NAME,XCB_GET_PROPERTY_TYPE_ANY,0,128);
				xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pcon,propertyCookie,0);
				if(!propertyReply)
					break; //TODO: get legacy XCB_ATOM_WM_NAME
				uint nameLen = xcb_get_property_value_length(propertyReply);
				char *pwmName = mstrndup((const char *)xcb_get_property_value(propertyReply),nameLen);
				pwmName[nameLen] = 0;

				BackendStringProperty prop(pwmName);
				PropertyChange(pclient1,PROPERTY_ID_WM_NAME,&prop);

				if(pclient1->pcontainer->titleBar != WManager::Container::TITLEBAR_NONE && nameLen > 0)
					pclient1->SetTitle1(prop.pstr);

				mstrfree(pwmName);

				free(propertyReply);

			}else
			if(pev->atom == XCB_ATOM_WM_CLASS){
				xcb_get_property_cookie_t propertyCookie = xcb_get_property(pcon,0,pev->window,XCB_ATOM_WM_CLASS,XCB_GET_PROPERTY_TYPE_ANY,0,128);
				xcb_get_property_reply_t *propertyReply = xcb_get_property_reply(pcon,propertyCookie,0);
				if(!propertyReply)
					break;
				uint classLen = xcb_get_property_value_length(propertyReply);
				char *pwmClass = mstrndup((const char *)xcb_get_property_value(propertyReply),classLen);
				pwmClass[classLen] = 0;

				BackendStringProperty prop(pwmClass);
				PropertyChange(pclient1,PROPERTY_ID_WM_CLASS,&prop);

				mstrfree(pwmClass);

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
			if(pev->window == pscr->root || pev->window == ewmh_window){
				if(pev->type == ewmh._NET_CURRENT_DESKTOP){
					//
					uint desktopIndex = pev->data.data32[0];
					printf("set desktop: %u\n",desktopIndex);
					if(desktopIndex >= WManager::Container::rootQueue.size()){
						char name[32];
						snprintf(name,sizeof(name),"%u",desktopIndex+1);
						CreateWorkspace(name)->Focus();
						break;
					}
					//
					WManager::Container *pcontainer = WManager::Container::rootQueue[desktopIndex];
					for(WManager::Container *pcontainer1 = pcontainer->GetFocus(); pcontainer1; pcontainer = pcontainer1, pcontainer1 = pcontainer1->GetFocus());

					if(pcontainer->pclient){
						X11Client *pclient11 = static_cast<X11Client *>(pcontainer->pclient);
						SetFocus(pclient11);
					}else pcontainer->Focus();
				}
				//if(pev->type == atoms[X11Backend::ATOM_CHAMFER_ALARM])
				//	TimerEvent();
				break;
			}

			X11Client *pclient1 = FindClient(pev->window,MODE_UNDEFINED);
			if(!pclient1 || pclient1->flags & X11Client::FLAG_UNMAPPING)
				break;

			if(pev->type == ewmh._NET_WM_STATE){
				if(pev->data.data32[1] == ewmh._NET_WM_STATE_FULLSCREEN){
					switch(pev->data.data32[0]){
					case 2://ewmh._NET_WM_STATE_TOGGLE)
						pclient1->pcontainer->Focus();
						SetFullscreen(pclient1,(pclient1->pcontainer->flags & WManager::Container::FLAG_FULLSCREEN) == 0);
						break;
					case 1://ewmh._NET_WM_STATE_ADD)
						pclient1->pcontainer->Focus();
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
			}else
			if(pev->type == ewmh._NET_ACTIVE_WINDOW){
				SetFocus(pclient1);
			}else
			if(pev->type == ewmh._NET_CLOSE_WINDOW){
				pclient1->Kill();
			}else
			if(pev->type == ewmh._NET_WM_MOVERESIZE){
				//---
			}else
			/*if(pev->type == ewmh._NET_WM_DESKTOP){
				uint desktopIndex = pev->data.data32[0];
				WManager::Container *pcontainer;
				if(desktopIndex >= WManager::Container::rootQueue.size()){
					char name[32];
					snprintf(name,sizeof(name),"%u",desktopIndex+1);
					pcontainer = CreateWorkspace(name);
					break;
				}else pcontainer = WManager::Container::rootQueue[desktopIndex];
				MoveContainer(pclient1->pcontainer,pcontainer);
			}else*/
			if(pev->type == ewmh._NET_MOVERESIZE_WINDOW){
				if(!(pclient1->pcontainer->flags & WManager::Container::FLAG_FLOATING))
					break; //for now only allow moving floating windows
				sint values[4] = {pev->data.data32[1],pev->data.data32[2],
					pev->data.data32[3],pev->data.data32[4]};
				xcb_configure_window(pcon,pclient1->window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y|XCB_CONFIG_WINDOW_WIDTH|XCB_CONFIG_WINDOW_HEIGHT,values);

				WManager::Rectangle rect = {values[0],values[1],values[2],values[3]};
				pclient1->UpdateTranslation(&rect);
			}
			result = 1;
			}
			break;
		case XCB_KEY_PRESS:{
			xcb_key_press_event_t *pev = (xcb_key_press_event_t*)pevent;
			lastTime = pev->time;
			/*if(pev->state & XCB_MOD_MASK_1 && pev->detail == testKeycode){
				//
				printf("test\n");
				xcb_grab_keyboard(pcon,0,pscr->root,lastTime,XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
			}else*/
			//Check if more than 10 minutes since previous input has elapsed since previous input (TODO: configurable).
			//This might indicate that the screen was turned off and everything has to be redrawn when scissoring.
			if(timespec_diff_sec(currentTime,inputTimer) > 600){
				WakeNotify();
				inputTimer = currentTime;
			}
			clock_gettime(CLOCK_MONOTONIC,&inputTimer);
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
			//printf("release: %x, %x\n",pev->state,pev->detail);
			for(KeyBinding &binding : keycodes){
				if(pev->state & binding.mask && pev->detail == binding.keycode){
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
			if(!pclient11 || pclient11->flags & X11Client::FLAG_UNMAPPING)
				break;

			pdragClient = pclient11;
			//dragClientX = pev->event_x; //client frame location
			//dragClientY = pev->event_y;
			dragRootX = pev->root_x;
			dragRootY = pev->root_y;

			xcb_grab_pointer(pcon,0,pscr->root,
				XCB_EVENT_MASK_BUTTON_RELEASE|XCB_EVENT_MASK_POINTER_MOTION,
				XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC,pscr->root,XCB_NONE,XCB_CURRENT_TIME);
			//xcb_allow_events(pcon,XCB_ALLOW_REPLAY_POINTER,pev->time);
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
			if(!pdragClient || pdragClient->flags & X11Client::FLAG_UNMAPPING)
				break;

			if(pdragClient->pcontainer->flags & WManager::Container::FLAG_FLOATING){
				sint values[2] = {pdragClient->rect.x+pev->event_x-dragRootX,pdragClient->rect.y+pev->event_y-dragRootY};
				xcb_configure_window(pcon,pdragClient->window,XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y,values);

				WManager::Rectangle rect = {values[0],values[1],pdragClient->rect.w,pdragClient->rect.h};
				pdragClient->UpdateTranslation(&rect);

			}else{
				pdragClient->pcontainer->canvasOffset += glm::vec2(pev->event_x-dragRootX,pev->event_y-dragRootY)
					/glm::vec2(pscr->width_in_pixels,pscr->height_in_pixels);
				pdragClient->pcontainer->Translate();
			}

			dragRootX = pev->event_x;
			dragRootY = pev->event_y;
			}
			break;
		case XCB_FOCUS_IN:{
			xcb_focus_in_event_t *pev = (xcb_focus_in_event_t*)pevent;
			printf("XCB_FOCUS_IN: *** focus %x\n",pev->event);

			X11Client *pclient11 = FindClient(pev->event,MODE_UNDEFINED);
			if(!pclient11 || pclient11->flags & X11Client::FLAG_UNMAPPING)
				break;

			if(standaloneComp)
				pfocusInClient = pclient11;
			}
			break;
		case XCB_ENTER_NOTIFY:{
			xcb_enter_notify_event_t *pev = (xcb_enter_notify_event_t*)pevent;
			lastTime = pev->time;

			X11Client *pclient1 = FindClient(pev->event,MODE_UNDEFINED);
			if(!pclient1)
				break;

			Enter(pclient1);

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

			auto m2 = std::find_if(configCache.begin(),configCache.end(),[&](auto &p)->bool{
				return std::get<0>(p) == pev->window;
			});
			if(m2 != configCache.end()){
				if(standaloneComp){
					//preserve the order
					auto m = configCache.erase(m2);
					if(m != configCache.begin())
						std::get<2>(*(m-1)) = (m != configCache.end())?std::get<0>(*m):0;
				}else{
					std::iter_swap(m2,configCache.end()-1);
					configCache.pop_back();
				}
			}

			debug_query_tree("DESTROY");

			//the removal below may not have been done yet if
			//the client got unmapped in another workspace.
			auto m1 = std::find_if(clients.begin(),clients.end(),[&](auto &p)->bool{
				return p.first->window == pev->window;
			});
			if(m1 == clients.end())
				break;

			//Erase here in case unmap was not notified. StackClients() will also take care of the wm mode erasure.
			(*m1).first->flags |= X11Client::FLAG_UNMAPPING; //should not be needed here
			clientStack.erase(std::remove(clientStack.begin(),clientStack.end(),(*m1).first),clientStack.end());

			unmappingQueue.insert((*m1).first);

			std::iter_swap(m1,clients.end()-1);
			clients.pop_back();
			}
			break;
		case 0:
			DebugPrintf(stdout,"Invalid event\n");
			break;
		default:
			//DebugPrintf(stdout,"default event: %u\n",pevent->response_type & 0x7f);

			X11Event event11(pevent,this);
			if(EventNotify(&event11))
				result = 1;

			break;
		}
		
		free(pevent);
		xcb_flush(pcon);
	}

	//Destroy the clients here, after the event queue has been cleared. This is to ensure that no already destroyed client is attempted to be readjusted.
	if(!unmappingQueue.empty()){
		std::set<WManager::Container *> roots;
		for(X11Client *pclient : unmappingQueue){
			DebugPrintf(stdout,"Unmap queue: %x\n",pclient->window);
			roots.insert(pclient->pcontainer->GetRoot());
			DestroyClient(pclient);
		}
		unmappingQueue.clear();

		if(!standaloneComp)
			for(auto m : roots)
				StackClients(m); //just to recreate the clientStack
	}

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

DebugClient::DebugClient(DebugContainer *pcontainer, const DebugClient::CreateInfo *pcreateInfo) : Client(pcontainer), pbackend(pcreateInfo->pbackend){
	UpdateTranslation();
	oldRect = rect;
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
	glm::vec4 coord = glm::vec4(pcontainer->p+pcontainer->margin*aspect,pcontainer->e-2.0f*pcontainer->margin*aspect)*screen;
	glm::vec2 titlePad = pcontainer->titlePad*aspect*glm::vec2(screen);
	//TODO: check if titlePad is larger than content
	//if(titlePad.x > coord.z)
	//
	glm::vec2 titlePadOffset = glm::min(titlePad,glm::vec2(0.0f));
	glm::vec2 titlePadExtent = glm::max(titlePad,glm::vec2(0.0f));

	if(!(pcontainer->flags & WManager::Container::FLAG_FULLSCREEN)){
		auto m = std::find(pcontainer->pParent->stackQueue.begin(),pcontainer->pParent->stackQueue.end(),pcontainer);
		glm::vec2 titleFrameExtent;
		if(pcontainer->pParent->flags & WManager::Container::FLAG_STACKED){
			stackIndex = m-pcontainer->pParent->stackQueue.begin();
			titleFrameExtent = glm::vec2(coord.z,coord.w)
				/(float)std::max((unsigned long)pcontainer->pParent->stackQueue.size(),1lu);
			titleStackOffset = (float)stackIndex*titleFrameExtent;
		}else{
			titleFrameExtent = glm::vec2(coord.z,coord.w);
			titleStackOffset = glm::vec2(0.0f);
		}

		coord -= glm::vec4(titlePadOffset.x,titlePadOffset.y,titlePadExtent.x-titlePadOffset.x,titlePadExtent.y-titlePadOffset.y);
		
		glm::vec2 titleFrameOffset = glm::vec2(coord)+titlePadOffset;
		if(titlePad.x > 1e-3f) //TODO: vectorize
			titleFrameOffset.x += coord.z;
		if(titlePad.y > 1e-3f)
			titleFrameOffset.y += coord.w;
		
		glm::vec2 titlePadAbs = glm::abs(titlePad);
		if(titlePadAbs.x > titlePadAbs.y){
			titleFrameOffset.y += titleStackOffset.y;
			titleFrameExtent.x = titlePadAbs.x;
		}else{
			titleFrameOffset.x += titleStackOffset.x;
			titleFrameExtent.y = titlePadAbs.y;
		}
	
		titleRect = (WManager::Rectangle){titleFrameOffset.x,titleFrameOffset.y,titleFrameExtent.x,titleFrameExtent.y};
		titlePad1 = titlePad;
	}
	
	oldRect = rect;
	clock_gettime(CLOCK_MONOTONIC,&translationTime);
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
	const_cast<X11Backend *>(pbackend)->StackClients(GetRoot());
}

void DebugContainer::SetClient(DebugClient *_pdebugClient){
	pdebugClient = _pdebugClient;
	pclient = _pdebugClient;
}

Debug::Debug() : X11Backend(false){
	//
}

Debug::~Debug(){
	//
}

void Debug::Start(){
	sint scount;
	pcon = xcb_connect(0,&scount);
	if(xcb_connection_has_error(pcon))
		throw Exception("Failed to connect to X server.");

	const xcb_setup_t *psetup = xcb_get_setup(pcon);
	xcb_screen_iterator_t sm = xcb_setup_roots_iterator(psetup);
	for(sint i = 0; i < scount; ++i)
		xcb_screen_next(&sm);

	pscr = sm.data;
	if(!pscr)
		throw Exception("Screen unavailable.");

	DebugPrintf(stdout,"Screen size: %ux%u (DPI: %fx%f)\n",pscr->width_in_pixels,pscr->height_in_pixels,25.4f*(float)pscr->width_in_pixels/(float)pscr->width_in_millimeters,25.4f*(float)pscr->height_in_pixels/(float)pscr->height_in_millimeters);

	psymbols = xcb_key_symbols_alloc(pcon);

	exitKeycode = SymbolToKeycode(XK_Q,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_1,exitKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	launchKeycode = SymbolToKeycode(XK_space,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_SHIFT,launchKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	closeKeycode = SymbolToKeycode(XK_X,psymbols);
	xcb_grab_key(pcon,1,pscr->root,XCB_MOD_MASK_SHIFT,closeKeycode,
		XCB_GRAB_MODE_ASYNC,XCB_GRAB_MODE_ASYNC);
	
	DefineBindings();

	xcb_flush(pcon);

	window = xcb_generate_id(pcon);

	uint values[1] = {XCB_EVENT_MASK_STRUCTURE_NOTIFY
		|XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
		|XCB_EVENT_MASK_KEY_PRESS
		|XCB_EVENT_MASK_KEY_RELEASE};
	xcb_create_window(pcon,XCB_COPY_FROM_PARENT,window,pscr->root,100,100,800,600,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,pscr->root_visual,XCB_CW_EVENT_MASK,values);
	const char title[] = "chamferwm compositor debug mode";
	xcb_change_property(pcon,XCB_PROP_MODE_REPLACE,window,XCB_ATOM_WM_NAME,XCB_ATOM_STRING,8,strlen(title),title);
	xcb_map_window(pcon,window);

	xcb_flush(pcon);
}

void Debug::Stop(){
	xcb_key_symbols_free(psymbols);
	//
	xcb_destroy_window(pcon,window);
	xcb_flush(pcon);
}

sint Debug::HandleEvent(bool forcePoll){
	for(xcb_generic_event_t *pevent = forcePoll?xcb_poll_for_event(pcon):xcb_wait_for_event(pcon);
		pevent; pevent = xcb_poll_for_event(pcon)){
		//switch(pevent->response_type & ~0x80){
		switch(pevent->response_type & 0x7f){
		/*case XCB_EXPOSE:{
			printf("expose\n");
			}
			break;*/
		case XCB_CLIENT_MESSAGE:{
			DebugPrintf(stdout,"message\n");
			//xcb_client_message_event_t *pev = (xcb_client_message_event_t*)pevent;
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
				if(pclient->pcontainer->titleBar != WManager::Container::TITLEBAR_NONE)
					pclient->SetTitle1("a relatively long debug title which has no other purpose but to test the limits of title rendering");
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
				if(pev->state & binding.mask && pev->detail == binding.keycode){
					KeyPress(binding.keyId,true);
					break;
				}
			}
			}
			break;
		case XCB_KEY_RELEASE:{
			xcb_key_release_event_t *pev = (xcb_key_release_event_t*)pevent;
			for(KeyBinding &binding : keycodes){
				if(pev->state & binding.mask && pev->detail == binding.keycode){
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

