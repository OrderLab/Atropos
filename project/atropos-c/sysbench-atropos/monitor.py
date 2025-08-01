import time
import os
import signal
import psutil
import socket
import sys 

def get_pid(name):
    pids = psutil.process_iter()
    for pid in pids:
        if(pid.name() == name):
            return pid.pid

fileName = "./output"
file = open(fileName, "rb")

# monitor static variable
minOutputLineSize = 107
defaultOutputLineSize = 118
estimateOutputLineSize = defaultOutputLineSize
defaultInterval = 0.1
monitoringDuration = 600
tpsThreshold = 10
latThreshold = 100

# socket
serverAddr = '/data/mysqldata/mysql.sock'

def clientSocket():
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            sock.connect(serverAddr)
        except socket.error as msg:
            print(msg)
            sys.exit(1)
        
        try:
            message = "This is the message."
            print("send ", message)
            sock.sendall(b'This is the message.')
            
        finally:
            print("close sock")
        
        sock.close()

class MonitorController:

    def __init__(self):
        self.NeedRelax = 0
        self.defaultWakeUpTime = 5
        self.WakeUpThreshold = 5/defaultInterval
        self.WakeUpCounter = 0
        self.targetPid = -1

    def shouldRelax(self):
        self.NeedRelax = 1

    def relaxing(self):
        self.WakeUpCounter += 1

    def tryWakeUp(self):
        if (self.WakeUpCounter < self.WakeUpThreshold):
            self.relaxing()
        else:
            self.NeedRelax = 0
            self.WakeUpCounter = 0

    def sendSignal(self):
        self.shouldRelax()
        clientSocket()

def parseLine(line):
    splitLine = line.split(" ")
    
    currentTime = float(splitLine[1][0:-1])
    currentTps = float(splitLine[6])
    currentLat = float(splitLine[13])

    return (currentTime, currentTps, currentLat)

def detect(line, controller):
    perf = parseLine(line)

    if (perf[1] < tpsThreshold and not controller.NeedRelax):
        controller.sendSignal()
        print("tps warning")
    elif (perf[2] > latThreshold and not controller.NeedRelax):
        controller.sendSignal()
        print("lat warning")
    elif(controller.NeedRelax):
        controller.tryWakeUp()

def monitorLUAOutput(controller):
    global defaultOutputLineSize
    global estimateOutputLineSize

    while True:
        file.seek(-estimateOutputLineSize, 2)
        lastLines = file.readlines()

        if len(lastLines) >= 2:
            line = str(lastLines[-1])

            # monitor could be over
            if (len(line) < minOutputLineSize):
                return False

            detect(line, controller)
            return True
        else:
            estimateOutputLineSize = estimateOutputLineSize + 2

count = 0

if __name__ == "__main__":

    controller = MonitorController()

    os.system("sudo sysbench /usr/local/share/sysbench/oltp_read_only.lua --mysql-socket=/data/mysqldata/mysql.sock --db-driver=mysql --mysql-port=3306 --mysql-user=root --mysql-password=adgjla --mysql-db=test --tables=3 --table-size=100000 --threads=2 --time=60 --report-interval=0.1 run > ./output 2>&1 &")

    time.sleep(5)
    while count < monitoringDuration:
        
        time.sleep(defaultInterval)
        hasOutput = monitorLUAOutput(controller)
        if not hasOutput:
            break
        count += 1