import os
import socket
from threading import Lock
from threading import Thread
import time
import operator
 
DEBUG=True
 
PORT = 4444
NUMPAGES = 1000000
MAXCONNREQUESTS = 5
 
# PERMISSION TYPES
NONE = "NONE"
READ = "READ"
WRITE = "WRITE"
 
class PageTableEntry:
  def __init__(self):
    self.lock = Lock()
    self.owner = NONE
    self.users = []
    self.current_permission = NONE
    self.invalidate_confirmations = {}
    self.b64_encoded_page = "EXISTING"
 
class ManagerServer:
  def __init__(self, port, numPages):
    self.port = port
    self.clients = {} # client ids => ip addresses
    self.page_table_entries = [PageTableEntry() for i in range(numPages)]

    self.serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.serverSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

 
  def Listen(self):
    self.serverSocket.bind(('0.0.0.0', self.port))
    self.serverSocket.listen(MAXCONNREQUESTS)
    # MAXCONNREQUESTS represents the number of outstanding clients waiting to
    # interact with the server before clients are turned away. It is 5 because
    # that is the normal default, but setting it to the # of clients is ok too.
    if DEBUG: print "[Manager Server] Waiting..."
 
    while True:
      # A client exists as long as the ManagerServer has a TCP connection to it.
      # Therefore, when a client connects here, we make a new thread to handle
      # its requests.
      (clientSocket, address) = self.serverSocket.accept()
      print 'received : ', address
      if DEBUG: print "[Manager] Accepted client with address: " + str(address)
 
      # Here, we just make a new thread to handle this client and run it, and
      # get back to waiting for new clients.
      clientThread = Thread(target = self.HandleClient, args = (clientSocket, ))
 
      client = clientSocket.getpeername()
      if DEBUG: print "[Manager] Client id " + str(client)
      self.AddClient(client, clientSocket)
 
      clientThread.start()
 
 
  # pass requests along to correct method in a new thread
  # serve Add/Remove clients requests
  # serve RequestPage requests
  # receive invalidate/sendpage confirmations
  # Call appropriate method in a new thread
  def HandleClient(self, clientSocket):
    # running in a new thread, handle the client
    client = clientSocket.getpeername()
    while True:
      try:
        data = clientSocket.recv(10, socket.MSG_PEEK)
        length = int(data.split(" ")[0])
        lengthoflength = data.find(" ") + 1
        data = clientSocket.recv(length + lengthoflength, socket.MSG_WAITALL)
        if not data: break
      except:
        break
 
      # print "manager received this: " + data
      if DEBUG: print "[Manager] " + str(client[1]) + " " + data[0:40]
      thread = Thread(target = self.ProcessMessage, args = (client, data.split(" ",1)[1]))
      thread.start()
 
    clientSocket.close()
 
 
  def ProcessMessage(self, client, data):
    args = data.split(" ")
    # print "manager side data:" + str(data)
    if args[0] == "REQUESTPAGE":
      self.RequestPage(client, int(args[1]) % NUMPAGES, args[3], data)
      # self.RequestPage(client, int(args[2]) , args[1])
    elif args[0] == "INVALIDATE_CONFIRMATION":
      b64_encoded_data = ""
      self.InvalidateConfirmation(client, int(args[1]) % NUMPAGES, b64_encoded_data)
    # elif args[0] == "INVALIDATE": //OLD
    #   b64_encoded_data = args[3] if len(args) > 3 else ""
    #   self.InvalidateConfirmation(client, int(args[2]) % NUMPAGES, b64_encoded_data)
    else:
      print "FUCK BAD PROTOCOL " + str(args[0])
 
 
  def AddClient(self, client, socket):
    print 'adding :', client, socket
    self.clients[client] = socket
 
  def Invalidate(self, client, pagenumber, getpage, permission):
    # Tell clients using the page to invalidate, wait for confirmation
    page_table_entry = self.page_table_entries[pagenumber]
    page_table_entry.invalidate_confirmations = {}
 
    for user in page_table_entry.users:
      if user != client:
        page_table_entry.invalidate_confirmations[user] = False
 
    for user in page_table_entry.users:
      if user != client:
        if getpage:
          self.Send(user, "INVALIDATE_COPYSET " + str(pagenumber) + " PAGEDATA " + permission + "$" + client[0] + "%" + client[1])
        else:
          self.Send(user, "INVALIDATE_COPYSET " + str(pagenumber) + " " +  permission  + "$" + client[0] + "%" + client[1])
 
    while not reduce(operator.and_, page_table_entry.invalidate_confirmations.values(), True):
      pass
 
    # When all have confirmed, return
 
  def InvalidateConfirmation(self, client, pagenumber, data):
    # Alert invalidate thread
    page_table_entry = self.page_table_entries[pagenumber]
    page_table_entry.invalidate_confirmations[client] = True
 
    # if data://OLD//TODO-CENT
    #   page_table_entry.b64_encoded_page = data
 
  def SendConfirmation(self, client, pagenumber, permission, b64_encoded_page):
    # print "send confirmation:manager side function -- " + str(client)
    self.Send(client, "RESPONSEPAGE " + str(pagenumber) + " " + str(permission) + " " + "-1 " + str(b64_encoded_page))
   
  def Send(self, client, msg):
    print  'manager:  ' + msg
    socket = self.clients[client]
    msg = str(len(msg)) + " " + msg
    print msg, client, socket
    socket.send(msg)
 
  def RequestPage(self, client, pagenumber, permission, msg):
    # Invalidate page with other clients (if necessary)
    # Make sure client has latests page, ask other client to send page if necessary
    page_table_entry = self.page_table_entries[pagenumber]
    page_table_entry.lock.acquire()

    # print "Inside RequestPage \n Permission: "+permission 
    # Initial use of page FAULT HANDLER
    if page_table_entry.current_permission == NONE:
      page_table_entry.current_permission = permission
      page_table_entry.users = [client]
      self.SendConfirmation(client, pagenumber, "READ", page_table_entry.b64_encoded_page)
 
      if permission == READ:
        page_table_entry.users= [client]

      page_table_entry.owner = client
      # print "Permission was None: Setting owner to "+str(client)
      page_table_entry.lock.release()
      return

    # READ FAULT HANDLER
    if permission == READ:
      # print "requesting page from "+ str(page_table_entry.owner)+" > "+msg
      self.Send(page_table_entry.owner, msg)

 
    # WRITE FAULT HANDLER
    if permission == WRITE:
      self.Send(page_table_entry.owner, msg)
      # print "setting owner to "+ str(client)+ " > "+msg
      page_table_entry.owner = client



    # # READ FAULT HANDLER
    # if permission == READ:
    #   if page_table_entry.current_permission == WRITE:
    #     self.Invalidate(client, pagenumber, True, permission)
    #     page_table_entry.users.append(client)
    #   else:
    #     page_table_entry.users.append(client)
 
    # # WRITE FAULT HANDLER
    # if permission == WRITE:
    #   if page_table_entry.current_permission == WRITE:
    #     self.Invalidate(client, pagenumber, True, permission)
    #   else:
    #     self.Invalidate(client, pagenumber, False, permission)
    #   page_table_entry.users = [client]
    #   page_table_entry.owner = client
 
    # self.SendConfirmation(client, pagenumber, permission, page_table_entry.b64_encoded_page)
    page_table_entry.current_permission = permission
    page_table_entry.lock.release()
 
if __name__ == "__main__":
  try:
    manager = ManagerServer(PORT, NUMPAGES)
    manager.Listen()
  except KeyboardInterrupt:
    for c, s in manager.clients.iteritems():
      s.close()
    os._exit(0)
