
import chamfer
import sys
from enum import Enum,auto

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

class Key(Enum):
	FOCUS_RIGHT = auto()
	FOCUS_LEFT = auto()
	FOCUS_UP = auto()
	FOCUS_DOWN = auto()
	FOCUS_PARENT = auto()
	FOCUS_CHILD = auto()
	FOCUS_FLOAT = auto()
	FOCUS_FLOAT_PREV = auto()

	FOCUS_PARENT_RIGHT = auto()
	FOCUS_PARENT_LEFT = auto()

	YANK_CONTAINER = auto()
	YANK_APPEND_CONTAINER = auto()
	PASTE_CONTAINER = auto()

	MOVE_LEFT = auto()
	MOVE_RIGHT = auto()

	LIFT_CONTAINER = auto()

	LAYOUT = auto()
	SPLIT_V = auto()
	FULLSCREEN = auto()

	CONTRACT_ROOT_RESET = auto()
	CONTRACT_ROOT_LEFT = auto() #contract left side
	CONTRACT_ROOT_RIGHT = auto() #contract right side
	EXPAND_ROOT_LEFT = auto()
	EXPAND_ROOT_RIGHT = auto() #right side

	CONTRACT_RESET = auto()
	CONTRACT_HORIZONTAL = auto()
	CONTRACT_VERTICAL = auto()
	EXPAND_HORIZONTAL = auto()
	EXPAND_VERTICAL = auto()

	KILL = auto()
	LAUNCH_TERMINAL = auto()
	LAUNCH_BROWSER = auto()
	LAUNCH_BROWSER_PRIVATE = auto()

	AUDIO_VOLUME_UP = auto()
	AUDIO_VOLUME_DOWN = auto()

	MONITOR_BRIGHTNESS_UP = auto()
	MONITOR_BRIGHTNESS_DOWN = auto()

	MODIFIER = auto()

	NOOP = auto()

class Container(chamfer.Container):
	#setup the container before it's created (dimensions)
	def OnSetupContainer(self):
		self.borderWidth = (0.015,0.015);
		self.minSize = (0.4,0.3);

		self.splitArmed = False;

		#if self.wm_class == "skype":
		#	self.floatingMode = chamfer.floatingMode.NEVER;
		if self.wm_class == "matplotlib":
			self.floatingMode = chamfer.floatingMode.ALWAYS;

	#setup the client before it's created (shaders)
	def OnSetupClient(self):
		#Panels, docks etc. should be rendered with no decorations. Later, it should be possible to check
		#this by looking at the window type property, not just the class name.
		if self.wm_class == "Conky":
			self.vertexShader = "default_vertex.spv";
			self.geometryShader = "default_geometry.spv";
			self.fragmentShader = "default_fragment.spv";
		else:
			self.vertexShader = "frame_vertex.spv";
			self.geometryShader = "frame_geometry.spv";
			self.fragmentShader = "frame_fragment.spv";

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
			print("created client, {} ({})".format(self.wm_name,self.wm_class));
		except UnicodeDecodeError:
			pass;
		self.Focus();

	#called to request fullscreen mode - either by calling SetFullscreen or by client message
	def OnFullscreen(self, toggle):
		#In fullscreen mode, no decorations
		if toggle:
			self.vertexShader = "default_vertex.spv";
			self.geometryShader = "default_geometry.spv";
			self.fragmentShader = "default_fragment.spv";
		else:
			self.vertexShader = "frame_vertex.spv";
			self.geometryShader = "frame_geometry.spv";
			self.fragmentShader = "frame_fragment.spv";
		self.ResetShaders(); #outside OnSetupClient, ResetShaders should be called to bind the new shaders

		return True;
	
	#called to request focus - either by calling container.Focus or by client message
	def OnFocus(self):
		return True;

	#called every time a client property has changed (title etc.)
	def OnPropertyChange(self, propId):
		print(self.wm_name);

	#called whenever cursor enters the window
	def OnEnter(self):
		#self.focus();
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
		
class Backend(chamfer.Backend):
	def OnSetupKeys(self, debug):
		if not debug:
			#setup key bindings
			#focusing clients
			self.BindKey(ord('l'),chamfer.MOD_MASK_1,Key.FOCUS_RIGHT.value);
			self.BindKey(ord('l'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.MOVE_RIGHT.value);
			self.BindKey(ord('h'),chamfer.MOD_MASK_1,Key.FOCUS_LEFT.value);
			self.BindKey(ord('h'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.MOVE_LEFT.value);
			self.BindKey(ord('k'),chamfer.MOD_MASK_1,Key.FOCUS_UP.value);
			self.BindKey(ord('j'),chamfer.MOD_MASK_1,Key.FOCUS_DOWN.value);
			self.BindKey(ord('a'),chamfer.MOD_MASK_1,Key.FOCUS_PARENT.value);
			self.BindKey(ord('s'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);
			self.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_1,Key.FOCUS_FLOAT.value);
			self.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.FOCUS_FLOAT_PREV.value);
			
			#reserved
			self.BindKey(ord('l'),chamfer.MOD_MASK_4,Key.FOCUS_PARENT_RIGHT.value);
			self.BindKey(ord('h'),chamfer.MOD_MASK_4,Key.FOCUS_PARENT_LEFT.value);

			#yanking and pasting containers
			self.BindKey(ord('y'),chamfer.MOD_MASK_1,Key.YANK_CONTAINER.value);
			self.BindKey(ord('y'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_CONTROL,Key.YANK_APPEND_CONTAINER.value);
			self.BindKey(ord('p'),chamfer.MOD_MASK_1,Key.PASTE_CONTAINER.value);

			#misc grouping and parenting operations
			self.BindKey(ord('w'),chamfer.MOD_MASK_1,Key.LIFT_CONTAINER.value);

			#layout, splits and fullscreen
			self.BindKey(ord('e'),chamfer.MOD_MASK_1,Key.LAYOUT.value);
			self.BindKey(latin1.XK_onehalf,chamfer.MOD_MASK_1,Key.SPLIT_V.value);
			self.BindKey(miscellany.XK_Tab,chamfer.MOD_MASK_4,Key.SPLIT_V.value);
			self.BindKey(ord('s'),chamfer.MOD_MASK_4,Key.SPLIT_V.value);
			self.BindKey(ord('f'),chamfer.MOD_MASK_1,Key.FULLSCREEN.value);

			#workspace dimensions
			self.BindKey(ord('r'),chamfer.MOD_MASK_4,Key.CONTRACT_ROOT_RESET.value);
			self.BindKey(ord('u'),chamfer.MOD_MASK_4,Key.CONTRACT_ROOT_LEFT.value);
			self.BindKey(ord('i'),chamfer.MOD_MASK_4,Key.CONTRACT_ROOT_RIGHT.value);
			self.BindKey(ord('u'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,Key.EXPAND_ROOT_LEFT.value);
			self.BindKey(ord('i'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,Key.EXPAND_ROOT_RIGHT.value);

			#client dimensions
			self.BindKey(ord('r'),chamfer.MOD_MASK_1,Key.CONTRACT_RESET.value);
			self.BindKey(latin1.XK_minus,chamfer.MOD_MASK_1,Key.CONTRACT_HORIZONTAL.value);
			self.BindKey(ord('j'),chamfer.MOD_MASK_4,Key.CONTRACT_HORIZONTAL.value);
			self.BindKey(latin1.XK_minus,chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.CONTRACT_VERTICAL.value);
			self.BindKey(ord('j'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,Key.CONTRACT_VERTICAL.value);
			self.BindKey(latin1.XK_plus,chamfer.MOD_MASK_1,Key.EXPAND_HORIZONTAL.value);
			self.BindKey(ord('k'),chamfer.MOD_MASK_4,Key.EXPAND_HORIZONTAL.value);
			self.BindKey(latin1.XK_plus,chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.EXPAND_VERTICAL.value);
			self.BindKey(ord('k'),chamfer.MOD_MASK_4|chamfer.MOD_MASK_SHIFT,Key.EXPAND_VERTICAL.value);
			
			#kill + launching applications
			self.BindKey(ord('q'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.KILL.value);
			self.BindKey(miscellany.XK_Return,chamfer.MOD_MASK_1,Key.LAUNCH_TERMINAL.value);
			self.BindKey(ord('1'),chamfer.MOD_MASK_4,Key.LAUNCH_BROWSER.value);
			self.BindKey(ord('2'),chamfer.MOD_MASK_4,Key.LAUNCH_BROWSER_PRIVATE.value);

			#volume
			self.BindKey(xf86.XK_XF86_AudioRaiseVolume,0,Key.AUDIO_VOLUME_UP.value);
			self.BindKey(xf86.XK_XF86_AudioLowerVolume,0,Key.AUDIO_VOLUME_DOWN.value);

			#monitor brightness
			self.BindKey(xf86.XK_XF86_MonBrightnessUp,0,Key.MONITOR_BRIGHTNESS_UP.value);
			self.BindKey(xf86.XK_XF86_MonBrightnessDown,0,Key.MONITOR_BRIGHTNESS_DOWN.value);

			#control mappings
			self.MapKey(XK.XK_Alt_L,chamfer.MOD_MASK_1,Key.MODIFIER.value);

			#hacks
			self.BindKey(ord('q'),chamfer.MOD_MASK_CONTROL,Key.NOOP.value); #prevent madness while browsing the web 

		else:
			#debug only
			self.BindKey(ord('h'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_LEFT.value);
			self.BindKey(ord('k'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_UP.value);
			self.BindKey(ord('l'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_RIGHT.value);
			self.BindKey(ord('j'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_DOWN.value);
			self.BindKey(ord('u'),chamfer.MOD_MASK_SHIFT,Key.MOVE_LEFT.value);
			self.BindKey(ord('i'),chamfer.MOD_MASK_SHIFT,Key.MOVE_RIGHT.value);
			self.BindKey(ord('a'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_PARENT.value);
			self.BindKey(ord('s'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_CHILD.value);
			self.BindKey(ord('y'),chamfer.MOD_MASK_SHIFT,Key.YANK_CONTAINER.value);
			self.BindKey(ord('y'),chamfer.MOD_MASK_SHIFT|chamfer.MOD_MASK_CONTROL,Key.YANK_APPEND_CONTAINER.value);
			self.BindKey(ord('p'),chamfer.MOD_MASK_SHIFT,Key.PASTE_CONTAINER.value);
			self.BindKey(ord('w'),chamfer.MOD_MASK_SHIFT,Key.LIFT_CONTAINER.value);
			self.BindKey(ord('e'),chamfer.MOD_MASK_SHIFT,Key.LAYOUT.value);
			self.BindKey(latin1.XK_onehalf,chamfer.MOD_MASK_SHIFT,Key.SPLIT_V.value);
	
	def OnCreateContainer(self):
		print("OnCreateContainer()");
		return Container();

	def GetFocusTiled(self):
		root = self.GetRoot();
		focusHead = root.GetFocus();
		focusPrev = None;
		while focusHead is not None:
			focusPrev = focusHead;
			focusHead = focusHead.GetFocus();
		return focusPrev;

	def OnKeyPress(self, keyId):
		print("key press: {}".format(keyId));
		focus = self.GetFocus();
		parent = focus.GetParent();

		if keyId == Key.FOCUS_RIGHT.value:
			self.shiftFocus = None;
			focus = self.GetFocusTiled() if focus.IsFloating() else focus.GetNext();
			#focus = focus.GetAdjacent(chamfer.adjacent.RIGHT);
			focus.Focus();

		elif keyId == Key.FOCUS_LEFT.value:
			self.shiftFocus = None;
			focus = self.GetFocusTiled() if focus.IsFloating() else focus.GetPrev();
			#focus = focus.GetAdjacent(chamfer.adjacent.LEFT);
			focus.Focus();

		elif keyId == Key.FOCUS_DOWN.value:
			self.shiftFocus = None;
			focus = focus.GetNext();
			#focus = focus.GetAdjacent(chamfer.adjacent.DOWN);
			focus.Focus();

		elif keyId == Key.FOCUS_UP.value:
			self.shiftFocus = None;
			focus = focus.GetPrev();
			#focus = focus.GetAdjacent(chamfer.adjacent.UP);
			focus.Focus();

		elif keyId == Key.MOVE_RIGHT.value:
			focus.MoveNext();

		elif keyId == Key.MOVE_LEFT.value:
			focus.MovePrev();

		elif keyId == Key.FOCUS_PARENT.value:
			if parent is None:
				return;
			parent.Focus();

		elif keyId == Key.FOCUS_CHILD.value:
			focus = focus.GetFocus();
			if focus is None:
				return;
			focus.Focus();

		elif keyId == Key.FOCUS_FLOAT.value:
			try:
				self.shiftFocus = self.shiftFocus.GetFloatFocus();
			except AttributeError:
				self.shiftFocus = focus.GetFloatFocus();

			if self.shiftFocus is None:
				return;
			self.GrabKeyboard(True);

			#TODO: need to draw a yellow "to-focus" frame, until alt is relased

		elif keyId == Key.FOCUS_FLOAT_PREV.value:
			#TODO, get previous from the focus history
			pass;

		elif keyId == Key.FOCUS_PARENT_RIGHT.value:
			if parent is None:
				return;
			parent1 = parent.GetNext();
			focus = parent1.GetFocus();
			if focus is None:
				return;
			focus.Focus();
			
		elif keyId == Key.FOCUS_PARENT_LEFT.value:
			if parent is None:
				return;
			parent1 = parent.GetPrev();
			focus = parent1.GetFocus();
			if focus is None:
				return;
			focus.Focus();
			
		elif keyId == Key.YANK_CONTAINER.value:
			print("yanking container...");
			self.yank = {focus};

		elif keyId == Key.YANK_APPEND_CONTAINER.value:
			print("yanking container (append)...");
			try:
				self.yank.add(focus);
			except AttributeError:
				self.yank = {focus};

		elif keyId == Key.PASTE_CONTAINER.value:
			print("pasting container...");
			try:
				if focus in self.yank:
					print("cannot paste on selection (one of the yanked containers).");
					self.yank.remove(focus);
				peers = list(self.yank);

				focus1 = focus.GetFocus();
				peers[0].Move(focus); #warning! need to know wether yank is still alive
				
				if focus1 is None:
					focus = focus.GetParent();
				for peer in peers[1:]:
					peer.Move(focus);
					
			except (AttributeError,IndexError):
				print("no containers to paste.");

		elif keyId == Key.LIFT_CONTAINER.value:
			sibling = focus.GetNext();
			peer = sibling.GetNext();
			if peer is not focus:
				sibling = peer.Place(sibling);
				
				peer = sibling.GetNext();
				while peer is not focus:
					peer1 = peer.GetNext();
					peer.Move(sibling);
					peer = peer1;

		elif keyId == Key.LAYOUT.value:
			if parent is None:
				return;
			layout = {
				chamfer.layout.VSPLIT:chamfer.layout.HSPLIT,
				chamfer.layout.HSPLIT:chamfer.layout.VSPLIT
			}[parent.layout];
			parent.ShiftLayout(layout);

		elif keyId == Key.SPLIT_V.value:
			#TODO: add render flags property, bitwise OR them
			print("split armed.");
			focus.splitArmed = not focus.splitArmed;

		elif keyId == Key.FULLSCREEN.value:
			print("setting fullscreen");
			focus.SetFullscreen(not focus.fullscreen);
		
		elif keyId == Key.CONTRACT_ROOT_RESET.value:
			root = self.GetRoot();
			root.canvasOffset = (0.0,0.0);
			root.canvasExtent = (0.0,0.0);
			root.ShiftLayout(root.layout);

		elif keyId == Key.CONTRACT_ROOT_LEFT.value:
			root = self.GetRoot();
			root.canvasOffset = (root.canvasOffset[0]+0.1,root.canvasOffset[1]);
			root.canvasExtent = (root.canvasExtent[0]+0.1,root.canvasExtent[1]);#root.canvasOffset;
			root.ShiftLayout(root.layout);

		elif keyId == Key.CONTRACT_ROOT_RIGHT.value:
			root = self.GetRoot();
			root.canvasExtent = (root.canvasExtent[0]+0.1,root.canvasExtent[1]);
			root.ShiftLayout(root.layout);

		elif keyId == Key.EXPAND_ROOT_LEFT.value:
			root = self.GetRoot();
			root.canvasOffset = (root.canvasOffset[0]-0.1,root.canvasOffset[1]);
			root.canvasExtent = (root.canvasExtent[0]-0.1,root.canvasExtent[1]);
			root.ShiftLayout(root.layout);

		elif keyId == Key.EXPAND_ROOT_RIGHT.value:
			root = self.GetRoot();
			root.canvasExtent = (root.canvasExtent[0]-0.1,root.canvasExtent[1]);
			root.ShiftLayout(root.layout);

		elif keyId == Key.CONTRACT_RESET.value:
			focus.canvasOffset = (0.0,0.0);
			focus.canvasExtent = (0.0,0.0);
			focus.ShiftLayout(focus.layout);

		elif keyId == Key.CONTRACT_HORIZONTAL.value:
			focus.canvasOffset = (focus.canvasOffset[0]+0.05,focus.canvasOffset[1]);
			focus.canvasExtent = (focus.canvasExtent[0]+0.10,focus.canvasExtent[1]);
			focus.ShiftLayout(focus.layout);

		elif keyId == Key.CONTRACT_VERTICAL.value:
			focus.canvasOffset = (focus.canvasOffset[0],focus.canvasOffset[1]+0.05);
			focus.canvasExtent = (focus.canvasExtent[0],focus.canvasExtent[1]+0.10);
			focus.ShiftLayout(focus.layout);

		elif keyId == Key.EXPAND_HORIZONTAL.value:
			focus.canvasOffset = (focus.canvasOffset[0]-0.05,focus.canvasOffset[1]);
			focus.canvasExtent = (focus.canvasExtent[0]-0.10,focus.canvasExtent[1]);
			focus.ShiftLayout(focus.layout);

		elif keyId == Key.EXPAND_VERTICAL.value:
			focus.canvasOffset = (focus.canvasOffset[0],focus.canvasOffset[1]-0.05);
			focus.canvasExtent = (focus.canvasExtent[0],focus.canvasExtent[1]-0.10);
			focus.ShiftLayout(focus.layout);

		elif keyId == Key.KILL.value:
			focus.Kill();

		elif keyId == Key.LAUNCH_TERMINAL.value:
			psutil.Popen(["termite"],stdout=None,stderr=None);

		elif keyId == Key.LAUNCH_BROWSER.value:
			psutil.Popen(["firefox"],stdout=None,stderr=None);

		elif keyId == Key.LAUNCH_BROWSER_PRIVATE.value:
			psutil.Popen(["firefox","--private-window"],stdout=None,stderr=None);

		elif keyId == Key.AUDIO_VOLUME_UP.value:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('volume-increaser') as pulse:
					for sink in pulse.sink_list():
						pulse.volume_change_all_chans(sink,0.05);

		elif keyId == Key.AUDIO_VOLUME_DOWN.value:
			if "pulsectl" in sys.modules:
				with pulsectl.Pulse('volume-increaser') as pulse:
					for sink in pulse.sink_list():
						pulse.volume_change_all_chans(sink,-0.05);

		elif keyId == Key.MONITOR_BRIGHTNESS_UP.value:
			psutil.Popen(["xbacklight","-inc","20"]);

		elif keyId == Key.MONITOR_BRIGHTNESS_DOWN.value:
			psutil.Popen(["xbacklight","-dec","20"]);

	def OnKeyRelease(self, keyId):
		print("key release: {}".format(keyId));

		if keyId == Key.MODIFIER.value:
			self.GrabKeyboard(False);
			try:
				self.shiftFocus.Focus();
			except AttributeError:
				pass;
	
	def OnTimer(self):
		battery = psutil.sensors_battery();
		try:
			if not battery.power_plugged:
				self.batteryFullNotified = False;
				if battery.percent <= 5 and self.batteryAlarmLevel < 3:
					psutil.Popen(["dunstify","--urgency=2","-p","100","Battery below 5%"]);
					self.batteryAlarmLevel = 3;
				elif battery.percent <= 10 and self.batteryAlarmLevel < 2:
					psutil.Popen(["dunstify","--urgency=2","-p","100","Battery below 10%"]);
					self.batteryAlarmLevel = 2;
				elif battery.percent <= 15 and self.batteryAlarmLevel < 1:
					psutil.Popen(["dunstify","--urgency=2","-p","100","Battery below 15%"]);
					self.batteryAlarmLevel = 1;
			else:
				self.batteryAlarmLevel = 0;
				if battery.percent >= 99 and not self.batteryFullNotified:
					psutil.Popen(["dunstify","--urgency=0","-p","100","Battery full"]);
					self.batteryFullNotified = True;
		except AttributeError:
			self.batteryFullNotified = True;
			self.batteryAlarmLevel = 0;

class Compositor(chamfer.Compositor):
	pass

backend = Backend();
chamfer.BindBackend(backend);

compositor = Compositor();
#compositor.deviceIndex = 0;
chamfer.BindCompositor(compositor);

pids = psutil.pids();
pnames = [psutil.Process(pid).name() for pid in pids];
pcmdls = [a for p in [psutil.Process(pid).cmdline() for pid in pids] for a in p];

#set wallpaper with feh
#psutil.Popen(["feh","--no-fehbg","--image-bg","black","--bg-center","background.png"]);

#if not "pulseaudio" in pnames:
#	print("starting pulseaudio...");
#	psutil.Popen(["sleep 1.0; pulseaudio --start"],shell=True,stdout=None,stderr=None);
#
#if not "dunst" in pnames:
#	print("starting dunst...");
#	psutil.Popen(["dunst"],stdout=None,stderr=None);
#
#if not any(["clipster" in p for p in pcmdls]):
#	print("starting clipster..."); #clipboard manager
#	psutil.Popen(["clipster","-d"],stdout=None,stderr=None);
#
#if not any(["libinput-gestures" in p for p in pcmdls]):
#	print("starting libinput-gestures..."); #touchpad gestures
#	psutil.Popen(["libinput-gestures-setup","start"],stdout=None,stderr=None);

