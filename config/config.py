
import chamfer
from enum import Enum,auto

from subprocess import Popen#,PIPE

class Key(Enum):
	FOCUS_RIGHT = auto()
	FOCUS_LEFT = auto()
	FOCUS_UP = auto()
	FOCUS_DOWN = auto()
	FOCUS_PARENT = auto()
	FOCUS_CHILD = auto()

	MOVE_LEFT = auto()
	MOVE_RIGHT = auto()

	LAYOUT = auto()
	SPLIT_V = auto()

	KILL = auto()
	LAUNCH_TERMINAL = auto()

class Container(chamfer.Container):
	def OnSetup(self):
		self.borderWidth = (0.015,0.015);
		self.minSize = (0.4,0.3);

		self.splitArmed = False;

	def OnParent(self):
		focus = chamfer.GetFocus();
		if hasattr(focus,'splitArmed') and focus.splitArmed:
			focus.splitArmed = False;
			return focus;

		parent = focus.GetParent();
		if parent is None:
			return focus; #root container

		return parent;
	
	def OnCreate(self):
		self.Focus();
	
	#def OnProperty(self):
		#called on property changes and once before creation
		#not called on containers without clients
		#pass;
		
class Backend(chamfer.Backend):
	def OnSetupKeys(self, binder, debug):
		print("setting up keys...");

		if not debug:
			binder.BindKey(ord('l'),chamfer.MOD_MASK_1,Key.FOCUS_RIGHT.value);
			binder.BindKey(ord('l'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.MOVE_RIGHT.value);
			binder.BindKey(ord('h'),chamfer.MOD_MASK_1,Key.FOCUS_LEFT.value);
			binder.BindKey(ord('h'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.MOVE_LEFT.value);
			binder.BindKey(ord('k'),chamfer.MOD_MASK_1,Key.FOCUS_UP.value);
			binder.BindKey(ord('j'),chamfer.MOD_MASK_1,Key.FOCUS_DOWN.value);
			binder.BindKey(ord('a'),chamfer.MOD_MASK_1,Key.FOCUS_PARENT.value);
			binder.BindKey(ord('s'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);
			binder.BindKey(ord('x'),chamfer.MOD_MASK_1,Key.FOCUS_CHILD.value);

			binder.BindKey(ord('e'),chamfer.MOD_MASK_1,Key.LAYOUT.value);
			binder.BindKey(chamfer.KEY_TAB,chamfer.MOD_MASK_1,Key.SPLIT_V.value);
			
			binder.BindKey(ord('q'),chamfer.MOD_MASK_1|chamfer.MOD_MASK_SHIFT,Key.KILL.value);
			binder.BindKey(chamfer.KEY_RETURN,chamfer.MOD_MASK_1,Key.LAUNCH_TERMINAL.value);

			#/ - search for a window
			#n - next match
			#N - previous match
			#[num] m - set window min size to [num]0%
			#... M - set max

			#alt+shift+a, select the top most base container under root

			#moving windows between containers: a cut/paste mechanism
			#not only indivudal clients can be replaced, but whole container hierarchies
		else:
			#debug only
			binder.BindKey(ord('h'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_LEFT.value);
			binder.BindKey(ord('k'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_UP.value);
			binder.BindKey(ord('l'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_RIGHT.value);
			binder.BindKey(ord('j'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_DOWN.value);
			binder.BindKey(ord('u'),chamfer.MOD_MASK_SHIFT,Key.MOVE_LEFT.value);
			binder.BindKey(ord('i'),chamfer.MOD_MASK_SHIFT,Key.MOVE_RIGHT.value);
			binder.BindKey(ord('a'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_PARENT.value);
			binder.BindKey(ord('s'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_CHILD.value);
			binder.BindKey(ord('x'),chamfer.MOD_MASK_SHIFT,Key.FOCUS_CHILD.value);

			binder.BindKey(ord('e'),chamfer.MOD_MASK_SHIFT,Key.LAYOUT.value);
			binder.BindKey(chamfer.KEY_TAB,chamfer.MOD_MASK_SHIFT,Key.SPLIT_V.value);
	
	def OnCreateContainer(self):
		print("OnCreateContainer()");
		return Container();

	def OnKeyPress(self, keyId):
		print("key press: {}".format(keyId));
		focus = chamfer.GetFocus();
		parent = focus.GetParent();
		#if parent is None:
			#return; #root container

#		if keyId == Key.FOCUS_RIGHT.value and parent.layout == chamfer.layout.VSPLIT:
#			#should GetNext() jump to the next container in parent if this is the last in this level?
#			#maybe additional GetNext2() for that
#			focus = focus.GetNext();
#			focus.Focus();
#
#		elif keyId == Key.FOCUS_LEFT.value and parent.layout == chamfer.layout.VSPLIT:
#			#focus = focus.GetAdjacent(chamfer.adjacent.LEFT);
#			focus = focus.GetPrev();
#			focus.Focus();
#
#		elif keyId == Key.FOCUS_DOWN.value and parent.layout == chamfer.layout.HSPLIT:
#			focus = focus.GetNext();
#			focus.Focus();
#
#		elif keyId == Key.FOCUS_UP.value and parent.layout == chamfer.layout.HSPLIT:
#			#focus = focus.GetAdjacent(chamfer.adjacent.LEFT);
#			focus = focus.GetPrev();
#			focus.Focus();
#
		if keyId == Key.FOCUS_RIGHT.value:
			focus = focus.GetNext();
			#focus = focus.GetAdjacent(chamfer.adjacent.RIGHT);
			focus.Focus();

		elif keyId == Key.FOCUS_LEFT.value:
			focus = focus.GetPrev();
			#focus = focus.GetAdjacent(chamfer.adjacent.LEFT);
			focus.Focus();

		elif keyId == Key.FOCUS_DOWN.value:
			focus = focus.GetNext();
			#focus = focus.GetAdjacent(chamfer.adjacent.DOWN);
			focus.Focus();

		elif keyId == Key.FOCUS_UP.value:
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
				return; #TODO: get first child if it exists
			focus.Focus();
			
		elif keyId == Key.LAYOUT.value:
			if parent is None:
				return;
			layout = {
				chamfer.layout.VSPLIT:chamfer.layout.HSPLIT,
				chamfer.layout.HSPLIT:chamfer.layout.VSPLIT
			}[parent.layout];
			parent.ShiftLayout(layout); #TODO: layout = ..., use UpdateLayout() to update all changes?
		
		elif keyId == Key.SPLIT_V.value:
			#arm the container split
			#TODO: add render flags property, bitwise or them
			print("split armed.");
			focus.splitArmed = not focus.splitArmed;

		elif keyId == Key.KILL.value:
			print("killing client...");
			focus.Kill();

		elif keyId == Key.LAUNCH_TERMINAL.value:
			Popen(["termite"],stdout=None,stderr=None);
	
	def OnKeyRelease(self, keyId):
		print("key release: {}".format(keyId));

class Compositor(chamfer.Compositor):
	def __init__(self):
		#self.shaderPath = "../";
		print(self.shaderPath);
	
	#def SetupGraphics(self):
	#	shader = self.LoadShader("frame_vertex.spv");

	def OnPropertyChange(self, prop):
		pass;

backend = Backend();
chamfer.bind_Backend(backend);

#compositor = Compositor();
#chamfer.bind_Compositor(compositor);

#startup applications here?

