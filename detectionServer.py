import sys
import cv2
import math
import time
import socket
import numpy as np
from PIL import Image
from imutils.object_detection import non_max_suppression

# Returns True if rectangle A (defined by topLeft_A and bottomRight_A)
# is inside rectangle B (defiend by topLeft_B and bottomRight_B) by a 10% margin
# ~~ topLeft_X and bottomRight_X are tuples of (x,y) ~~
def isInside(topLeft_A, bottomRight_A, topLeft_B, bottomRight_B):
	return (topLeft_A[0] * 1.10) >= topLeft_B[0] and bottomRight_A[1] <= (bottomRight_B[1] * 1.10)

# Returns a new list of rectangles that contains no nested rectangles
# A rectangle is a tuple of (xA, yA, xB, yB), where A is the top left corner
# of a rectangle and B is the bottom right corner
def filterNested(rects):
	new_rects = []

	for R1 in rects:
		nested = False
		for R2 in rects:
			if tuple(R1) == tuple(R2):
				continue
			if isInside((R1[0], R1[1]), (R1[2], R1[3]), (R2[0], R2[1]), (R2[2], R2[3])):
				nested = True
				break
		if nested == False:
			new_rects.append(R1)
	
	return new_rects
		

	

def MAX_COLOR(img, topLeft, bottomRight):
	

	colors = {}
	ORIGIN = (round((bottomRight[0] - topLeft[0])/2) + topLeft[0], round((bottomRight[1] - topLeft[1])/2) + topLeft[1])
	
	for i in range(topLeft[0], bottomRight[0] + 1):   # x iterator
		#print("I: ", i)
		for j in range(topLeft[1] + 1, bottomRight[1]):   # y iterator
			if i < 0 or i >= img.shape[1] or j < 0 or j >= img.shape[0]:
				continue
			#print("J: ", j)
			dist = round(math.sqrt(abs(ORIGIN[0]-i)**2 + abs((ORIGIN[1]-j)**2)))  # distance magnitude
			strength = 1/(1 + dist)  # closeness to origin = strength

			color_key = str(img[j][i][0]) + ":" + str(img[j][i][1]) + ":" + str(img[j][i][2])


			#print(color_key)
			if color_key in colors:
				colors[color_key] += strength
			else:
				colors[color_key] = strength
	
	# colors_sorted = dict(sorted(colors.items(), key=lambda item: item[1], reverse=True))
	# f = open('colorDetector.txt', 'w')
	# f.write(str(colors_sorted))

	max_color = max(colors, key=colors.get)

	return max_color

def IS_BLUE(max_color_key):
	max_color_str_vector = max_color_key.split(':')
	
	red = int(max_color_str_vector[2])
	green = int(max_color_str_vector[1])
	blue = int(max_color_str_vector[0])

	return (blue > 100 and blue > green and (blue + green) > red * 2)

def filterByColor(img, people, color):
	filtered = []

	for person in people:
		xA, yA, xB, yB = person
		max_color_key = MAX_COLOR(img, (xA, yA), (xB, yB))	
		if color == 'BLUE' and IS_BLUE(max_color_key):
			filtered.append(person)
		if color == 'RED' and IS_RED(max_color_key):
			filtered.append(person)
	return filtered


def IS_RED(max_color_key):
	max_color_str_vector = max_color_key.split(':')
	
	red = int(max_color_str_vector[2])
	green = int(max_color_str_vector[1])
	blue = int(max_color_str_vector[0])

	return (red > 100 and red > green * 2 and red > blue * 2)

def bbox_to_centroid(bbox):
	x, y, w, h = bbox
	return (int(x + (w/2)), int(y + (h/2)))




# Enable camera
cap = cv2.VideoCapture(1)
cap.set(3, 640)
cap.set(4, 360)

frame_count = 0

# import cascade file for facial recognition
faceCascade = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")

hog = cv2.HOGDescriptor()
hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())

locked = False



if 'BLUE' in sys.argv or 'B' in sys.argv:
	print('Applying BLUE filter!')
	color = 'BLUE'

if 'RED' in sys.argv or 'R' in sys.argv:
	print('Applying RED filter!')
	color = 'RED'

tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_socket.bind(('0.0.0.0', 2123))
tcp_socket.listen()
tcp_server, addr = tcp_socket.accept()

udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
client_addr = ('192.168.1.37', 3284)
#client_addr = ('127.0.0.1', 3284)
print('Attempting to connect...')
try:
	udp_socket.sendto(b'Welcome!', client_addr)
except socket.error:
	print('UDP Connection could not be established!')

while True:
	success, img = cap.read()
	frame_count += 1

	if frame_count > 150 or locked == False:
		imgGray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

		# Getting corners around the face
		faces = faceCascade.detectMultiScale(imgGray, 1.3, 5)  # 1.3 = scale factor, 5 = minimum neighbor
		faces = np.array([[x, y, x + w, y + (4*h)] for (x, y, w, h) in faces])

		bodies, weights = hog.detectMultiScale(imgGray, winStride=(8,8))
		bodies = non_max_suppression(bodies, probs=None, overlapThresh=0.001)
		bodies = np.array([[x, y, x + w, y + h] for (x, y, w, h) in bodies])
		bodies = filterNested(bodies)

		bodies.extend(faces)

		bodies = filterByColor(img, bodies, color)



		bodies_bbox = list((xA, yA, (abs(xB-xA)), (abs(yB-yA))) for (xA, yA, xB, yB) in bodies)
		
		# for (xA, yA, xB, yB) in bodies:
		# 	cv2.rectangle(img, (xA, yA), (xB, yB), (0, 255, 255), 2)
		
		if locked == True:
			if len(bodies) < 1:
				locked = False
				try:
					tcp_server.send(b'unlock')
				except:
					print('TCP Connection Lost!')
					tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
					tcp_socket.bind(('', 8085))
					tcp_socket.listen()
					tcp_server, addr = tcp_socket.accept()
			frame_count = 0
			continue
			

		if len(bodies) > 0:
			tracker = cv2.legacy_TrackerMOSSE.create()
			tracker.init(img, bodies_bbox[0])
			# print('TARGET ACQUIRED AT: ', bbox_to_centroid(bodies_bbox[0]))
			locked = True
			try:
				tcp_server.send(b'lock')
			except:
				print('TCP Connection lost!')
				tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
				tcp_socket.bind(('', 8085))
				tcp_socket.listen()
				tcp_server, addr = tcp_socket.accept()
			(lock_x, lock_y) = bbox_to_centroid(bodies_bbox[0])
			msg = str(lock_x) + ',' + str(lock_y)
			msg = bytes(msg, 'utf-8')
			try:
				udp_socket.sendto(msg, client_addr)
			except:
				print('UDP Connection could be established!')

	
	else:
		(success, bbox) = tracker.update(img)

		if success:
			#print('TARGET LOCKED: ', bbox)
			#(x, y, w, h) = [int(v) for v in bbox]
			(lock_x, lock_y) = bbox_to_centroid(bbox)
			msg = str(lock_x) + ',' + str(lock_y)
			msg = bytes(msg, 'utf-8')
			try:
				udp_socket.sendto(msg, client_addr)
			except:
				print('UDP Connection could be established!')

			print('TARGET LOCKED AT: ', bbox_to_centroid(bbox))
			

			#cv2.rectangle(img, (x, y), (x + w, y + h), (0, 0, 255), 2)
		
		else:
			print('LOCK LOST!')
			locked = False
			try:
				tcp_server.send(b'unlock')
			except:
				print('TCP Connection Lost!')
				tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
				tcp_socket.bind(('', 8085))
				tcp_socket.listen()
				tcp_server, addr = tcp_socket.accept()
			frame_count = 0
			continue
	
cap.release()
tcp_socket.close()
udp_socket.close()
