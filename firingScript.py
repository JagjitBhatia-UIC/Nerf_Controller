import RPi.GPIO as GPIO
import time
import socket
import logging

logging.basicConfig(filename="firingLog.txt", level=logging.DEBUG)
logging.captureWarnings(True)


HOST = ''		# localhost
PORT = 5687

flywheel = 15
firing = 16

# Switch on and off due to relay 
on = GPIO.LOW
off = GPIO.HIGH

GPIO.setmode(GPIO.BOARD)
GPIO.setup(flywheel, GPIO.OUT)
GPIO.setup(firing, GPIO.OUT)

GPIO.output(flywheel, off)
GPIO.output(firing, off)

print('Starting firing script....\n')



while True:
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
		print('just to see whats happening')
		sock.bind((HOST, PORT))
		sock.listen()
		client, addr = sock.accept()
		with client:
			print('Connected by', addr)
			while True:
				cmd = client.recv(1024)
				print('Received command: ', cmd)
				if not cmd:
					break
				
				if cmd == b'FIRING_ON':
					print('Initiating firing....')
					GPIO.output(flywheel, on)	
					time.sleep(1)	# Sleep for 1 second to fully rev up flywheel
					GPIO.output(firing, on)
				
				elif cmd == b'FIRING_OFF':
					print('Terminating firing....')
					GPIO.output(firing, off)
					time.sleep(0.5)
					GPIO.output(flywheel, off)
				
				elif cmd == b'QUIT':
					print('Quitting program....')
					quit()

