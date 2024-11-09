import socket
import sys
import Socket
import Event
import time
import Grid
import Client
import subprocess

g_client_sockets = []

def StartServices():
    print("Starting jack")
    subprocess.call("QT_QPA_PLATFORM=offscreen qjackctl &", shell=True)
    time.sleep(10)
    print("Starting carla")
    subprocess.call("~/start_carla.sh", shell=True)

if __name__ == "__main__":
    Grid.InitGrid()
    mgr = Client.ClientManager()
    StartServices()
    mgr.DoLoop()
