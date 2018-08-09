
import autopy
import time
import copy

from subprocess import Popen, PIPE

import Config

autoit = None

try:
	import win32com.client
	import pywintypes
	try:
		autoit = win32com.client.Dispatch("AutoItX3.Control")
		autoit.Opt("SendKeyDownDelay", 50)
		autoit.Opt("SendKeyDelay", 50)
	except pywintypes.com_error:
		pass
except ImportError:
	pass

class Colors:

	#                  [][]        []      []          [][]        []    [][]
	#                  [][]      [][][]    [][][]    [][]      [][][]      [][]    [][][][]
	MINO_ACTIVE   = [0xffc225, 0xd24cad, 0x4464e9, 0x7cd424, 0xff7e25, 0xfa325a, 0x32befa]
	MINO_INACTIVE = [0xe39f02, 0xaf298a, 0x2141c6, 0x59b101, 0xe35b02, 0xd70f37, 0x0f9bd7]

	BOARD_1 = 0x2B2B2B
	BOARD_2 = 0x2F2F2F

	WALL           =  0x999999
	BOMB          = [0xff9900, 0xffff00]

class Types:
	
	INACTIVE_MINO = 1
	ACTIVE_MINO   = 2
	WALL          = 3
	BOMB          = 4
	OTHER         = 0

class TetrisState:

	board = None
	next_type = -1
	hold_type = -1
	active_type = -1

class TetrisStateReader:
	
	def __init__(self, outstate):
		self.out = outstate
		self.last_x = 0
		self.last_y = 0
		pass
	
	def read_state(self):
		capture = autopy.bitmap.capture_screen()
		found = False
		if self.is_board(capture, self.last_x, self.last_y):
			return True
		for y in range(0, capture.height - 16):
			for x in range(57, capture.width - 256):
				if capture.get_color(x, y) == Colors.BOARD_1:
					if self.is_board(capture, x, y):
						self.last_x = x
						self.lasy_y = y
						return True
		return False

	def is_board(self, capture, x, y):
		for cy in range(y, y + 16, 1):
			for cx in range(x, x + 16, 1):
				if capture.get_color(cx, cy) != Colors.BOARD_1:
					return False
		for cy in range(y, y + 16, 1):
			for cx in range(x + 18, x + 16 + 18, 1):
				if capture.get_color(cx, cy) != Colors.BOARD_2:
					return False
		while x - 18 > 0:
			previous_color = capture.get_color(x - 18 + 8, y + 8)
			if (previous_color in [Colors.BOARD_1, Colors.BOARD_2]
			or  previous_color in Colors.MINO_ACTIVE
			or  previous_color in Colors.MINO_INACTIVE):
				x -= 18
			else:
				break
		self.scan_board(x, y, capture)
		self.out.next_type = self.scan_small(x + 216, y + 30, capture)
		self.out.hold_type = self.scan_small(x - 57, y + 30, capture)
		return True

	def scan_small(self, origin_x, origin_y, capture):
		rect = ((origin_x, origin_y), (31, 31))
		for x in range(rect[0][0], rect[0][0] + rect[1][0]):
			for y in range(rect[0][1], rect[0][1] + rect[1][1]):
				color = capture.get_color(x, y)
				index = 0
				for pattern in Colors.MINO_ACTIVE:
					if color == pattern:
						return index
					index += 1
		return -1

	def scan_board(self, origin_x, origin_y, capture):
		lines = []
		self.out.active_type = -1
		for row in range(0, 20):
			y = origin_y + row * 18 + 8
			line = []
			has_wall = False
			for column in range(0, 10):
				x = origin_x + column * 18 + 8
				color = capture.get_color(x, y)
				if color == Colors.WALL:
					has_wall = True
					break
			for column in range(0, 10):
				x = origin_x + column * 18 + 8
				color = capture.get_color(x, y)
				index = 0
				if self.out.active_type == -1:
					for pattern in Colors.MINO_ACTIVE:
						if color == pattern:
							self.out.active_type = index
						index += 1
				if color in Colors.MINO_ACTIVE:
					line.append(Types.ACTIVE_MINO)
				elif color in Colors.MINO_INACTIVE:
					line.append(Types.INACTIVE_MINO)
				elif has_wall:
					if color == Colors.WALL:
						line.append(Types.WALL)
					else:
						line.append(Types.BOMB)
				else:
					line.append(Types.OTHER)
			lines.append(line)
		self.out.board = lines

class TetrisController:
	
	def __init__(self):
		self.last_key = None
		pass
	
	def left(self):
		self.tap(autopy.key.K_LEFT, "{Left}")
	
	def right(self):
		self.tap(autopy.key.K_RIGHT, "{Right}")

	def drop(self):
		time.sleep(0.06)
		self.tap(' ', ' ')
		time.sleep(0.15)
		
	def hold(self):
		self.tap('z', 'z')

	def rotate(self):
		self.tap(autopy.key.K_UP, "{Up}")

	def tap(self, k, wk):
		if autoit:
			autoit.Send(wk)
			return
		if self.last_key == k:
			time.sleep(Config.UP_TIME)
		self.last_key = k
		autopy.key.toggle(k, True)
		time.sleep(Config.DOWN_TIME)
		autopy.key.toggle(k, False)

class TetrisBot:

	def __init__(self, PlayerClass):
		self.state = TetrisState()
		self.state_reader = TetrisStateReader(self.state)
		self.controller = TetrisController()
		self.player = PlayerClass()
		while True:
			if self.state_reader.read_state():
				if self.state.board:
					self.player.process_state(self.state, self.controller)
			time.sleep(0.05)


class TetrisPlayer:

	def __init__(self):
		self.process = Popen([Config.BOT_PATH], stdin=PIPE, stdout=PIPE)

	def write(self, text):
		if Config.SHOW_INPUT:
			print ' >>> ', text.strip()
		self.process.stdin.write(text)

	def process_state(self, state, controller):
		self.state = state
		self.controller = controller
		for line in state.board:
			self.write((' '.join([str(cell) for cell in line])) + '\n')
		self.write(str(state.active_type) + '\n');
		self.write(str(state.next_type) + '\n');
		self.write(str(state.hold_type) + '\n');
		self.process.stdin.flush();
		
		command_dict = {
			'#left':   controller.left,
			'#right':  controller.right,
			'#drop':   controller.drop,
			'#hold':   controller.hold,
			'#rotate': controller.rotate
		};

		while True:
			line = self.process.stdout.readline()
			if line == "":
				print "THE BOT DIED!!!!!!111!!1"
				raise IOError()
			else:
				line = line.strip()
				if Config.SHOW_OUTPUT:
					print ' <<< ', line
				if line == "#finish":
					break
				elif line in command_dict:
					command_dict[line]()

if __name__ == '__main__':
	TetrisBot(TetrisPlayer)

