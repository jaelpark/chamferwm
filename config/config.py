
import chamfer
import sys
import os
from enum import Enum,auto

class ShaderFlag(Enum):
	FOCUS_NEXT = chamfer.shaderFlag.USER_BIT<<0x0

#Refer to the documentation
#https://jaelpark.github.io/chamferwm-docs/bindings.html
#for all the keybindings and how to setup them.
#TLDR: define the key binding in OnSetupKeys() and the
#behaviour in OnKeyPress().

class Container(chamfer.Container):
	#setup the container before it's created (dimensions)
	def OnSetupContainer(self):
		self.margin = (0.015,0.015); #Relative amount of empty space around containers (i.e. gap). Fragment shader covers this area for over-reaching decorations.
		self.minSize = (0.3,0.2); #Minimum size of the container, before they start to overlap

		self.titleBar = chamfer.titleBar.TOP; #options for the title bar location are NONE, LEFT, TOP, RIGHT or BOTTOM
		self.titleStackOnly = True; #disable to always show title bars

		#WM_CLASS/TITLE rules to force floating mode / never float
		if self.wm_class == "matplotlib":
			#Example: always float matplotlib graph windows.
			self.floatingMode = chamfer.floatingMode.ALWAYS; #.NEVER

	#setup the client before it's created (shaders)
	def OnSetupClient(self):
		try:
			print("Setting up \"{}\" ({})".format(self.wm_name,self.wm_class),flush=True);
		except UnicodeDecodeError:
			pass;

		#Setup shaders for the newly created client container.
		#The stock build includes the following fragment shaders
		#build by default:

		#1. frame_fragment.spv: default "demo" decoration with border and rounded corners
		#2. frame_fragment_basic.spv: rectangular borders with focus highlight, no bling
		#3. frame_fragment_ext.spv: #default rounded corner style for external wms
		#4. frame_fragment_ext_basic.spv: #simple compatible style for external wms, only draw shadows
		#5. default_fragment.spv: #absolutely no decoration (goes together with default_vertex.spv and
		#   (default_geometry.spv)

		#All shader builds include the shadows by default. Shadows can be
		#removed by disabling the build feature in meson.build. Other features
		#such as the border thickness, rounding, and colors can be edited
		#in the shader source itself (for now).

		if not chamfer.backend.standaloneCompositor:
			#Shaders for the chamferwm.
			if self.wm_class == "Conky":
				#Example: zero decoration for conky system monitor.
				#Add docks and other desktop widgets similarly,
				#if needed.
				self.vertexShader = "default_vertex.spv";
				self.geometryShader = "default_geometry.spv";
				self.fragmentShader = "default_fragment.spv";
			else:
				self.vertexShader = "frame_vertex.spv";
				self.geometryShader = "frame_geometry.spv";
				self.fragmentShader = "frame_fragment.spv";
		else:
			#Shader for other window managers (standalone
			#compositor mode).
			self.vertexShader = "frame_vertex.spv";
			self.geometryShader = "frame_geometry.spv";
			self.fragmentShader = "frame_fragment_ext.spv";

		#this is just to restore the defaults when leaving fullscreen (used in OnFullscreen())
		self.defaultShaders = (self.vertexShader,self.geometryShader,self.fragmentShader);

	#select and assign a parent container
	def OnParent(self):
		focus = chamfer.backend.GetFocus();
		if hasattr(focus,'splitArmed') and focus.splitArmed:
			focus.splitArmed = False;
			return focus;

		parent = focus.GetParent();
		if parent is None:
			return focus; #root container

		return parent;

	#called once client has been created and mapped to display
	def OnCreate(self):
		try:
			print("Created client \"{}\" ({})".format(self.wm_name,self.wm_class),flush=True);
		except UnicodeDecodeError:
			pass
		self.Focus();

	#Called to request fullscreen mode - either by calling SetFullscreen or by client message.
	#Not used in standalone compositor mode.
	def OnFullscreen(self, toggle):
		#In fullscreen mode, no decorations.
		if toggle:
			self.vertexShader = "default_vertex.spv";
			self.geometryShader = "default_geometry.spv";
			self.fragmentShader = "default_fragment.spv";
		else:
			self.vertexShader,self.geometryShader,self.fragmentShader = self.defaultShaders;

		return True;
	
	#called to request focus - either by calling container.Focus or by client message
	def OnFocus(self):
		return True;

	#called when window is stacked or unstacked - only called for the parent container
	def OnStack(self, toggle):
		pass;

	#called every time a client property has changed (title etc.)
	def OnPropertyChange(self, propId):
		pass; #self.wm_name, etc.

	#called whenever cursor enters the window
	def OnEnter(self):
		#self.Focus();
		pass;
	
	#Place container under another so that its structure remains intact.
	#Return the container at the original targets place.
	def Place(self, target):
		focus = target.GetFocus();
		if focus is not None:
			peers = [focus.GetNext()];
			while peers[-1] is not focus:
				peers.append(peers[-1].GetNext());

		self.Move(target);
		if focus is not None:
			for peer in peers[1:]:
				peer.Place(peers[0]);
			return target;
		else:
			return target.GetParent();
		
	def GetFocusDescend(self):
		#get the focused container in this container tree
		container = self;
		while container.GetFocus() is not None:
			container = container.GetFocus();
		return container;
	
	def FindParentLayout(self, layout):
		try:
			container = self;
			while container.GetParent().layout != layout:
				container = container.GetParent();
			return container;

		except AttributeError:
			return self;
	
class Backend(chamfer.Backend):
	def OnSetupKeys(self, debug):
		#Helper function to create key binding IDs
		def KeyId(keyId):
			nonlocal self;
			if hasattr(self,keyId):
				return getattr(self,keyId);
			try:
				self.keyCounter += 1;
				setattr(self,keyId,self.keyCounter);
			except AttributeError:
				self.keyCounter = 0;
				setattr(self,keyId,0);
			return self.keyCounter;

		if not debug:
			self.modMask = chamfer.MOD_MASK_1;
			#setup key bindings
			#focusing clients
			self.BindKey(ord('l'),self.modMask,KeyId('FOCUS_RIGHT'));
			self.BindKey(ord('l'),self.modMask|chamfer.MOD_MASK_SHIFT,KeyId('MOVE_RIGHT'));
			self.BindKey(ord('h'),self.modMask,KeyId('FOCUS_LEFT'));
			self.BindKey(ord('h'),self.modMask|chamfer.MOD_MASK_SHIFT,KeyId('MOVE_LEFT'));
			self.BindKey(ord('k'),self.modMask,KeyId('FOCUS_UP'));
			self.BindKey(ord('j'),self.modMask,KeyId('FOCUS_DOWN'));
			self.BindKey(ord('a'),self.modMask,KeyId('FOCUS_PARENT'));
			self.BindKey(ord('s'),self.modMask,KeyId('FOCUS_CHILD'));
			self.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_4,KeyId('FOCUS_MRU'));
			self.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_1,KeyId('FOCUS_FLOAT'));
			self.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_FLOAT_PREV'));
			
			#reserved
			self.BindKey(ord('l'),chamfer.MOD_MASK_4,KeyId('FOCUS_PARENT_RIGHT'));
			self.BindKey(ord('h'),chamfer.MOD_MASK_4,KeyId('FOCUS_PARENT_LEFT'));

			#yanking and pasting containers
			self.BindKey(ord('y'),self.modMask,KeyId('YANK_CONTAINER'));
			self.BindKey(ord('y'),self.modMask|chamfer.MOD_MASK_CONTROL,KeyId('YANK_APPEND_CONTAINER'));
			self.BindKey(ord('p'),self.modMask,KeyId('PASTE_CONTAINER'));

			#misc grouping and parenting operations
			self.BindKey(ord('w'),self.modMask,KeyId('LIFT_CONTAINER'));

			#layout, splits and fullscreen
			self.BindKey(ord('e'),self.modMask,KeyId('LAYOUT'));
			self.BindKey(latin1.XK_onehalf,self.modMask,KeyId('SPLIT_V'));
			self.BindKey(ord('v'),self.modMask,KeyId('SPLIT_V'));
			self.BindKey(ord('f'),self.modMask,KeyId('FULLSCREEN'));
			self.BindKey(ord('t'),self.modMask,KeyId('STACK'));
			self.BindKey(ord('l'),self.modMask|chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('STACK_RIGHT'));
			self.BindKey(ord('h'),self.modMask|chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('STACK_LEFT'));
			self.BindKey(latin1.XK_space,self.modMask,KeyId('FLOAT'));

			#workspace dimensions
			self.BindKey(ord('r'),chamfer.MOD_MASK_4,KeyId('CONTRACT_ROOT_RESET'));
			self.BindKey(ord('u'),chamfer.MOD_MASK_4,KeyId('CONTRACT_ROOT_LEFT'));
			self.BindKey(ord('i'),chamfer.MOD_MASK_4,KeyId('CONTRACT_ROOT_RIGHT'));
			self.BindKey(ord('u'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,KeyId('EXPAND_ROOT_LEFT'));
			self.BindKey(ord('i'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,KeyId('EXPAND_ROOT_RIGHT'));

			#client dimensions
			self.BindKey(ord('r'),self.modMask,KeyId('CONTRACT_RESET'));

			self.BindKey(latin1.XK_minus,self.modMask,KeyId('CONTRACT_HORIZONTAL'));
			self.BindKey(ord('j'),chamfer.MOD_MASK_4,KeyId('CONTRACT_HORIZONTAL'));
			self.BindKey(latin1.XK_minus,self.modMask|chamfer.MOD_MASK_CONTROL,KeyId('CONTRACT_HORIZONTAL_LOCAL'));
			self.BindKey(ord('j'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_CONTROL,KeyId('CONTRACT_HORIZONTAL_LOCAL'));

			self.BindKey(latin1.XK_minus,self.modMask|chamfer.MOD_MASK_SHIFT,KeyId('CONTRACT_VERTICAL'));
			self.BindKey(ord('j'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,KeyId('CONTRACT_VERTICAL'));
			self.BindKey(latin1.XK_minus,self.modMask|chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('CONTRACT_VERTICAL_LOCAL'));
			self.BindKey(ord('j'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('CONTRACT_VERTICAL_LOCAL'));

			self.BindKey(latin1.XK_plus,self.modMask,KeyId('EXPAND_HORIZONTAL'));
			self.BindKey(ord('k'),chamfer.MOD_MASK_4,KeyId('EXPAND_HORIZONTAL'));
			self.BindKey(latin1.XK_plus,self.modMask|chamfer.MOD_MASK_CONTROL,KeyId('EXPAND_HORIZONTAL_LOCAL'));
			self.BindKey(ord('k'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_CONTROL,KeyId('EXPAND_HORIZONTAL_LOCAL'));

			self.BindKey(latin1.XK_plus,self.modMask|chamfer.MOD_MASK_SHIFT,KeyId('EXPAND_VERTICAL'));
			self.BindKey(ord('k'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,KeyId('EXPAND_VERTICAL'));
			self.BindKey(latin1.XK_plus,self.modMask|chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('EXPAND_VERTICAL_LOCAL'));
			self.BindKey(ord('k'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('EXPAND_VERTICAL_LOCAL'));

			#workspaces (regular and composed)
			self.BindKey(ord('1'),self.modMask,KeyId('WORKSPACE_1'));
			self.BindKey(ord('2'),self.modMask,KeyId('WORKSPACE_2'));
			self.BindKey(ord('3'),self.modMask,KeyId('WORKSPACE_3'));
			self.BindKey(ord('4'),self.modMask,KeyId('WORKSPACE_4'));
			#one special workspace without compositor (maximize performance for fullscreen games, video and such)
			self.BindKey(ord('0'),self.modMask,KeyId('WORKSPACE_NOCOMP'));
			
			#kill + launching applications (setup your default programs in the respective OnKeyPress if-branch)
			self.BindKey(ord('q'),self.modMask|chamfer.MOD_MASK_SHIFT,KeyId('KILL'));
			self.BindKey(miscellany.XK_Return,self.modMask,KeyId('LAUNCH_TERMINAL'));
			self.BindKey(ord('1'),chamfer.MOD_MASK_4,KeyId('LAUNCH_BROWSER'));
			self.BindKey(ord('2'),chamfer.MOD_MASK_4,KeyId('LAUNCH_BROWSER_PRIVATE'));

			#volume
			self.BindKey(xf86.XK_XF86_AudioMute,0,KeyId('AUDIO_VOLUME_MUTE'));
			self.BindKey(xf86.XK_XF86_AudioRaiseVolume,0,KeyId('AUDIO_VOLUME_UP'));
			self.BindKey(xf86.XK_XF86_AudioLowerVolume,0,KeyId('AUDIO_VOLUME_DOWN'));

			#screenshots
			self.BindKey(ord('s'),chamfer.MOD_MASK_4,KeyId('SCREENSHOT'));

			#monitor brightness
			self.BindKey(xf86.XK_XF86_MonBrightnessUp,0,KeyId('MONITOR_BRIGHTNESS_UP'));
			self.BindKey(xf86.XK_XF86_MonBrightnessDown,0,KeyId('MONITOR_BRIGHTNESS_DOWN'));

			#control mappings
			self.MapKey(XK.XK_Alt_L,chamfer.MOD_MASK_1,KeyId('MODIFIER_1'));
			self.MapKey(XK.XK_Super_L,chamfer.MOD_MASK_4,KeyId('MODIFIER_4'));

			#hacks
			self.BindKey(ord('q'),chamfer.MOD_MASK_CONTROL,KeyId('NOOP')); #prevent madness while web browsing (disable Ctrl+Q by overriding it)

		else:
			#debug only (compositor testing mode)
			self.BindKey(ord('h'),chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_LEFT'));
			self.BindKey(ord('k'),chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_UP'));
			self.BindKey(ord('l'),chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_RIGHT'));
			self.BindKey(ord('j'),chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_DOWN'));
			self.BindKey(ord('u'),chamfer.MOD_MASK_SHIFT,KeyId('MOVE_LEFT'));
			self.BindKey(ord('i'),chamfer.MOD_MASK_SHIFT,KeyId('MOVE_RIGHT'));
			self.BindKey(ord('a'),chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_PARENT'));
			self.BindKey(ord('s'),chamfer.MOD_MASK_SHIFT,KeyId('FOCUS_CHILD'));
			self.BindKey(ord('y'),chamfer.MOD_MASK_SHIFT,KeyId('YANK_CONTAINER'));
			self.BindKey(ord('y'),chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,KeyId('YANK_APPEND_CONTAINER'));
			self.BindKey(ord('p'),chamfer.MOD_MASK_SHIFT,KeyId('PASTE_CONTAINER'));
			self.BindKey(ord('w'),chamfer.MOD_MASK_SHIFT,KeyId('LIFT_CONTAINER'));
			self.BindKey(ord('e'),chamfer.MOD_MASK_SHIFT,KeyId('LAYOUT'));
			self.BindKey(ord('t'),chamfer.MOD_MASK_SHIFT,KeyId('STACK'));
			self.BindKey(latin1.XK_onehalf,chamfer.MOD_MASK_SHIFT,KeyId('SPLIT_V'));
	
	def OnCreateContainer(self):
		print("OnCreateContainer()",flush=True);
		return Container();

	def OnKeyPress(self, keyId):
		focus = self.GetFocus();
		parent = focus.GetParent();

		if keyId == self.FOCUS_RIGHT:
			focus = focus.GetTiledFocus() if focus.IsFloating() else focus.FindParentLayout(chamfer.layout.VSPLIT).GetNext().GetFocusDescend();
			focus.Focus();

		elif keyId == self.FOCUS_LEFT:
			focus = focus.GetTiledFocus() if focus.IsFloating() else focus.FindParentLayout(chamfer.layout.VSPLIT).GetPrev().GetFocusDescend();
			focus.Focus();

		elif keyId == self.FOCUS_DOWN:
			focus = focus.GetTiledFocus() if focus.IsFloating() else focus.FindParentLayout(chamfer.layout.HSPLIT).GetNext().GetFocusDescend();
			focus.Focus();

		elif keyId == self.FOCUS_UP:
			focus = focus.GetTiledFocus() if focus.IsFloating() else focus.FindParentLayout(chamfer.layout.HSPLIT).GetPrev().GetFocusDescend();
			focus.Focus();

		elif keyId == self.MOVE_RIGHT:
			focus.MoveNext();

		elif keyId == self.MOVE_LEFT:
			focus.MovePrev();

		elif keyId == self.FOCUS_PARENT:
			if parent is None:
				return;
			parent.Focus();

		elif keyId == self.FOCUS_CHILD:
			focus = focus.GetFocus();
			if focus is None:
				return;
			focus.Focus();

		elif keyId == self.FOCUS_MRU:
			try:
				self.shiftFocus.shaderFlags &= ~ShaderFlag.FOCUS_NEXT.value;
			except AttributeError:
				pass;

			try:
				self.shiftFocus = self.shiftFocus.GetTiledFocus();
			except AttributeError:
				self.shiftFocus = focus.GetTiledFocus();

			if self.shiftFocus is None:
				return;
			self.shiftFocus.shaderFlags |= ShaderFlag.FOCUS_NEXT.value;

			self.GrabKeyboard(True);

		elif keyId == self.FOCUS_FLOAT:
			try:
				self.shiftFocus.shaderFlags &= ~ShaderFlag.FOCUS_NEXT.value;
			except AttributeError:
				pass;

			try:
				self.shiftFocus = self.shiftFocus.GetFloatFocus();
			except AttributeError:
				self.shiftFocus = focus.GetFloatFocus();

			if self.shiftFocus is None:
				return;
			self.shiftFocus.shaderFlags |= ShaderFlag.FOCUS_NEXT.value;

			self.GrabKeyboard(True);

		elif keyId == self.FOCUS_FLOAT_PREV:
			#TODO, get previous from the focus history
			pass;

		elif keyId == self.FOCUS_PARENT_RIGHT:
			if parent is None:
				return;
			parent1 = parent.GetNext();
			focus = parent1.GetFocus();
			if focus is None:
				return;
			focus.Focus();
			
		elif keyId == self.FOCUS_PARENT_LEFT:
			if parent is None:
				return;
			parent1 = parent.GetPrev();
			focus = parent1.GetFocus();
			if focus is None:
				return;
			focus.Focus();
			
		elif keyId == self.YANK_CONTAINER:
			print("yanking container...",flush=True);
			self.yank = {focus};

		elif keyId == self.YANK_APPEND_CONTAINER:
			print("yanking container (append)...",flush=True);
			try:
				self.yank.add(focus);
			except AttributeError:
				self.yank = {focus};

		elif keyId == self.PASTE_CONTAINER:
			print("pasting container...",flush=True);
			try:
				if focus in self.yank:
					print("cannot paste on selection (one of the yanked containers).",flush=True);
					self.yank.remove(focus);
				peers = list(self.yank);

				focus1 = focus.GetFocus();
				peers[0].Move(focus);
				
				if focus1 is None:
					focus = focus.GetParent();
				for peer in peers[1:]:
					peer.Move(focus);
					
			except (AttributeError,IndexError):
				print("no containers to paste.",flush=True);
			except ValueError:
				print("expired container.",flush=True);

		elif keyId == self.LIFT_CONTAINER:
			sibling = focus.GetNext();
			peer = sibling.GetNext();
			if peer is not focus:
				sibling = peer.Place(sibling);
				
				peer = sibling.GetNext();
				while peer is not focus:
					peer1 = peer.GetNext();
					peer.Move(sibling);
					peer = peer1;

		elif keyId == self.LAYOUT:
			if parent is None:
				return;
			layout = {
				chamfer.layout.VSPLIT:chamfer.layout.HSPLIT,
				chamfer.layout.HSPLIT:chamfer.layout.VSPLIT
			}[parent.layout];
			parent.ShiftLayout(layout);

		elif keyId == self.SPLIT_V:
			try:
				#Indicate that next time a container is created,
				#the current focus will be split.
				focus.splitArmed = not focus.splitArmed;
			except AttributeError:
				focus.splitArmed = True;

		elif keyId == self.FULLSCREEN:
			print("setting fullscreen",flush=True);
			focus.SetFullscreen(not focus.fullscreen);

		elif keyId == self.STACK:
			if parent is None:
				return;
			parent.SetStacked(not parent.stacked);

		elif keyId == self.STACK_RIGHT:
			container = focus.FindParentLayout(chamfer.layout.VSPLIT).GetNext();
			if container is focus:
				return;
			focus.Move(container);

			parent1 = focus.GetParent();
			parent1.SetStacked(True);
			parent1.ShiftLayout(chamfer.layout.HSPLIT);

		elif keyId == self.STACK_LEFT:
			container = focus.FindParentLayout(chamfer.layout.VSPLIT).GetPrev();
			if container is focus:
				return;
			focus.Move(container);

			parent1 = focus.GetParent();
			parent1.SetStacked(True);
			parent1.ShiftLayout(chamfer.layout.HSPLIT);

		elif keyId == self.FLOAT:
			focus.SetFloating(True);
		
		elif keyId == self.CONTRACT_ROOT_RESET:
			root = self.GetRoot();
			root.canvasOffset = (0.0,0.0);
			root.canvasExtent = (0.0,0.0);
			root.ShiftLayout(root.layout);

		elif keyId == self.CONTRACT_ROOT_LEFT:
			root = self.GetRoot();
			root.canvasOffset = (root.canvasOffset[0]+0.1,root.canvasOffset[1]);
			root.canvasExtent = (root.canvasExtent[0]+0.1,root.canvasExtent[1]);#root.canvasOffset;
			root.ShiftLayout(root.layout);

		elif keyId == self.CONTRACT_ROOT_RIGHT:
			root = self.GetRoot();
			root.canvasExtent = (root.canvasExtent[0]+0.1,root.canvasExtent[1]);
			root.ShiftLayout(root.layout);

		elif keyId == self.EXPAND_ROOT_LEFT:
			root = self.GetRoot();
			root.canvasOffset = (root.canvasOffset[0]-0.1,root.canvasOffset[1]);
			root.canvasExtent = (root.canvasExtent[0]-0.1,root.canvasExtent[1]);
			root.ShiftLayout(root.layout);

		elif keyId == self.EXPAND_ROOT_RIGHT:
			root = self.GetRoot();
			root.canvasExtent = (root.canvasExtent[0]-0.1,root.canvasExtent[1]);
			root.ShiftLayout(root.layout);

		elif keyId == self.CONTRACT_RESET:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.canvasOffset = (0.0,0.0);
			container.canvasExtent = (0.0,0.0);
			container.ShiftLayout(container.layout);

		elif keyId == self.CONTRACT_HORIZONTAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.size = (container.size[0]-0.1,container.size[1]);

		elif keyId == self.CONTRACT_HORIZONTAL_LOCAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.canvasOffset = (container.canvasOffset[0]+0.05,container.canvasOffset[1]);
			container.canvasExtent = (container.canvasExtent[0]+0.10,container.canvasExtent[1]);
			container.ShiftLayout(container.layout);

		elif keyId == self.CONTRACT_VERTICAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.size = (container.size[0],container.size[1]-0.1);

		elif keyId == self.CONTRACT_VERTICAL_LOCAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.canvasOffset = (container.canvasOffset[0],container.canvasOffset[1]+0.05);
			container.canvasExtent = (container.canvasExtent[0],container.canvasExtent[1]+0.10);
			container.ShiftLayout(container.layout);

		elif keyId in [self.WORKSPACE_1,self.WORKSPACE_2,self.WORKSPACE_3,self.WORKSPACE_4]:
			#add more workspaces by defining more WORKSPACE_*
			wsName = str(keyId-self.WORKSPACE_1+1);
			root = self.GetRoot(wsName);
			root.GetFocusDescend = Container.GetFocusDescend.__get__(root); #RootContainer does not have GetFocusDescend, so we will add it now
			root.GetFocusDescend().Focus();

		elif keyId == self.WORKSPACE_NOCOMP:
			root = self.GetRoot("nocomp");
			root.GetFocusDescend = Container.GetFocusDescend.__get__(root);
			root.GetFocusDescend().Focus();

		elif keyId == self.EXPAND_HORIZONTAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.size = (container.size[0]+0.1,container.size[1]);

		elif keyId == self.EXPAND_HORIZONTAL_LOCAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.canvasOffset = (container.canvasOffset[0]-0.05,container.canvasOffset[1]);
			container.canvasExtent = (container.canvasExtent[0]-0.10,container.canvasExtent[1]);
			container.ShiftLayout(container.layout);

		elif keyId == self.EXPAND_VERTICAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.size = (container.size[0],container.size[1]+0.1);

		elif keyId == self.EXPAND_VERTICAL_LOCAL:
			container = parent if (parent is not None) and parent.stacked else focus;
			container.canvasOffset = (container.canvasOffset[0],container.canvasOffset[1]-0.05);
			container.canvasExtent = (container.canvasExtent[0],container.canvasExtent[1]-0.10);
			container.ShiftLayout(container.layout);

		elif keyId == self.KILL:
			focus.Kill();

		elif keyId == self.LAUNCH_TERMINAL:
			for t in [os.getenv("TERMINAL"),"alacritty","kitty","urxvt","rxvt","st","xterm"]:
				try:
					psutil.Popen([t],stdout=None,stderr=None);
					break;
				except (TypeError,FileNotFoundError):
					pass;

		elif keyId == self.LAUNCH_BROWSER:
			psutil.Popen(["firefox"],stdout=None,stderr=None);

		elif keyId == self.LAUNCH_BROWSER_PRIVATE:
			psutil.Popen(["firefox","--private-window"],stdout=None,stderr=None);

		elif keyId == self.AUDIO_VOLUME_MUTE:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('chamferwm') as pulse:
					defSinkName = pulse.server_info().default_sink_name;
					for sink in pulse.sink_list():
						if sink.name == defSinkName:
							pulse.mute(sink,not sink.mute);
							break;

		elif keyId == self.AUDIO_VOLUME_UP:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('chamferwm') as pulse:
					defSinkName = pulse.server_info().default_sink_name;
					for sink in pulse.sink_list():
						if sink.name == defSinkName:
							pulse.volume_change_all_chans(sink,0.05);
							break;

		elif keyId == self.AUDIO_VOLUME_DOWN:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('chamferwm') as pulse:
					defSinkName = pulse.server_info().default_sink_name;
					for sink in pulse.sink_list():
						if sink.name == defSinkName:
							pulse.volume_change_all_chans(sink,-0.05);
							break;

		elif keyId == self.SCREENSHOT:
			psutil.Popen(["sh","-c","import png:- | xclip -selection clipboard -t image/png"]);

		elif keyId == self.MONITOR_BRIGHTNESS_UP:
			psutil.Popen(["xbacklight","-inc","20"]);

		elif keyId == self.MONITOR_BRIGHTNESS_DOWN:
			psutil.Popen(["xbacklight","-dec","20"]);

	def OnKeyRelease(self, keyId):
		if keyId == self.MODIFIER_1 or keyId == self.MODIFIER_4:
			self.GrabKeyboard(False);
			try:
				self.shiftFocus.shaderFlags &= ~ShaderFlag.FOCUS_NEXT.value;
				self.shiftFocus.Focus(); #ignoreTimer=True
			except AttributeError:
				pass;
	
			self.shiftFocus = None;

class Compositor(chamfer.Compositor):
	def OnRedirectExternal(self, title, className):
		#Used only in standalone compositor mode. Return False to filter out incompatible WM/UI components.
		return True;

backend = Backend();
chamfer.BindBackend(backend);

if not backend.standaloneCompositor:
	#Import some modules for WM use
	try:
		import psutil
	except ModuleNotFoundError:
		print("No psutil module.");

	try:
		from Xlib.keysymdef import latin1,miscellany,xf86
		from Xlib import XK
	except ModuleNotFoundError:
		print("No Xlib module.");

	try:
		import pulsectl
	except ModuleNotFoundError:
		print("No pulsectl module.");

compositor = Compositor();
#compositor.deviceIndex = 0;
compositor.fontName = "Monospace";
compositor.fontSize = 24;
compositor.enableAnimation = True;
chamfer.BindCompositor(compositor);

if not backend.standaloneCompositor:
	#Acquire list of running processes to not launch something that is already running.
	pids = psutil.pids();
	pnames = [psutil.Process(pid).name() for pid in pids];
	pcmdls = [a for p in [psutil.Process(pid).cmdline() for pid in pids] for a in p];

	#---set wallpaper with feh
	#psutil.Popen(["feh","--no-fehbg","--image-bg","black","--bg-center","background.png"]);

	#---startup programs examples:
	#launch notification system
	#if not "dunst" in pnames:
	#	print("starting dunst...");
	#	psutil.Popen(["dunst"],stdout=None,stderr=None);

	#launch clipboard manager
	#if not any(["clipster" in p for p in pcmdls]):
	#	print("starting clipster..."); #clipboard manager
	#	psutil.Popen(["clipster","-d"],stdout=None,stderr=None);

	#launch gestures daemon for touch devices
	#if not any(["libinput-gestures" in p for p in pcmdls]):
	#	print("starting libinput-gestures..."); #touchpad gestures
	#	psutil.Popen(["libinput-gestures-setup","start"],stdout=None,stderr=None);

