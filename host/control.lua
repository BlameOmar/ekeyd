-- daemon/setup.lua
--
-- Entropy Key Initialisation, configuration and management code.
--
-- Copyright 2009 Simtec Electronics

-- The low-level C API provided to us.
local addfd = _addfd
local delfd = _delfd
local writefd = _writefd
local nowritefd = _nowritefd
local ekey_add = _add_ekey
local ekey_del = _del_ekey
local ekey_query = _query_ekey
local ekey_stat = _stat_ekey
local read_keys = _load_keys
local open_output_file = _open_output_file
local open_kernel_output = _open_kernel_output
local open_foldback_output = _open_foldback_output
local daemonise = _daemonise
local unlink = _unlink
local chmod = _chmod
local chown = _chown
local enumerate = _enumerate
local gc = collectgarbage
local debugprint = function() end -- Use print to output debugging

if debugprint == print then
   -- if we're debugging, ensure we don't daemonise by default.
   -- a Daemonise(true) in the config will override this.
   _daemonise(false)
end

-- Constants and useful values
local PROTOCOL_VERSION = "1"

-- DOS Protection
local dos_callcount = 0

-- Libraries we need
require "socket"

local have_unix_domain_sockets = false
function tryload_unix()
   require "socket.unix"
   have_unix_domain_sockets = true
end

pcall(tryload_unix)

local protectedenv = {}

-- Control socket interface
local controlsockets = {}
local ctlwritebuffers = {}
local ctlreadbuffers = {}
local ctlpendingcmd = {}
local ctltokill = {}
local ctldielater = {}
local ctltoaccept = {}
local ctlenv = {}
local ctlrhandler = {}

function addctlsocket(sock, name, isconn, rhandler)
   --print("New " .. (isconn and "control" or "connection") .. " socket: " .. name)
   controlsockets[sock] = name
   controlsockets[name] = sock
   ctlreadbuffers[sock] = ""
   ctltoaccept[sock] = not isconn
   addfd(sock:getfd())
   local newenv = {}
   local newmeta = {__index=protectedenv}
   setmetatable(newenv, newmeta)
   ctlenv[sock] = newenv
   ctlrhandler[sock] = rhandler
end

function delctlsocket(sock)
   local name = controlsockets[sock]
   if (type(name) ~= "string") then
      name, sock = sock, name
   end
   controlsockets[sock] = nil
   controlsockets[name] = nil
   if ctlwritebuffers[sock] then
      ctlwritebuffers[sock] = nil
      nowritefd(sock:getfd())
   end
   ctlreadbuffers[sock] = nil
   ctlpendingcmd[sock] = nil
   ctltokill[sock] = nil
   ctltoaccept[sock] = nil
   ctldielater[sock] = nil
   ctlenv[sock] = nil
   ctlrhandler[sock] = nil
   delfd(sock:getfd())
   sock:close()
end

function ctlwrite(sockorname, str)
   local sock = type(sockorname) == "string" and controlsockets[sockorname] or sockorname
   local curw = ctlwritebuffers[sock]
   
   if str == nil or str == "" then
      -- Pointless write attempt.
      return
   end
   
   ctlwritebuffers[sock] = (curw and curw or "") .. str
   if not curw then
      writefd(sock:getfd())
   end
end

function _ctldowrite(sock)
   -- Yes, this one always gets a socket
   local str = ctlwritebuffers[sock]
   if (ctltokill[sock]) then return end
   local written = sock:send(str)
   if (written == 0) then return end
   debugprint("Written:",tostring(written))
   if written == nil then
      ctltokill[sock] = true
   end
   if written == #str then
      ctlwritebuffers[sock] = nil
      nowritefd(sock:getfd())
      if ctldielater[sock] then
	 ctltokill[sock] = true
      end
      return
   end
   ctlwritebuffers[sock] = string.sub(str, written + 1)
end

function _ctldoread(sock)
   -- Yes, this one always gets a socket too
   if ctlrhandler[sock] then
      return ctlrhandler[sock](sock)
   end
   -- Not got a specific handler, so check on and assume it's a control socket
   if ctltoaccept[sock] then
      local client = sock:accept()
      --print "Done accept"
      client:settimeout(0.010)
      --print "Done timeout"
      pcall(function() client:setoption("tcp-nodelay", true) end)
      --print "Done option"
      local peer, maybeport = "UNIX", 0
      pcall(function() peer, maybeport = client:getpeername() end)
      --print "Done peername"
      if maybeport then peer = peer .. ":" .. tostring(maybeport) end
      --print "made peer"
      local name = "C:" .. peer
      --print "made name"
      addctlsocket(client, name, true)
      --print "ctlsocket added"
      ctlwrite(client, "PROTOCOL EKEYD/" .. PROTOCOL_VERSION .. "\n")
      return
   end
   local r,msg = sock:receive()
   if r == nil or r == 0 then
      ctltokill[sock] = true
      return
   end
   ctlpendingcmd[sock] = r
end

-- Output management
local output_configured = false

-- Entropy Key management

local ekey_list = {}
local ekey_nr = 1

local failmodes = {
   ["0"] = { "efm_ok", "No failure" },
   ["1"] = { "efm_raw_left_bad", "Left generator is no longer random" },
   ["2"] = { "efm_raw_right_bad", "Right generator is no longer random" },
   ["3"] = { "efm_raw_xor_bad", "Generators have become correlated" },
   ["4"] = { "efm_debias_left_bad", "Left generator is strongly biassed" },
   ["5"] = { "efm_debias_right_bad", "Right generator is strongly biassed" },
   ["7"] = { "efm_temp_too_low", "Temperature detected below threshold" },
   ["8"] = { "efm_temp_too_high", "Temperature detected above threshold" },
   ["9"] = { "efm_fips1402_threshold_exceeded", "FIPS 140-2 tests exceeded threshold for failed blocks"},
   ["A"] = { "efm_volt_too_low", "Voltage too low" },
   ["B"] = { "efm_volt_too_high", "Voltage too high" }}

local function update_ekey_stat(ekey)
   ekey.stats = ekey_stat(ekey.ekey)
   ekey.stats["EntropyRate"] = math.floor((ekey.stats["TotalEntropy"] * 8) / ekey.stats["ConnectionTime"])
   ekey.stats["KeyTemperatureK"] = ekey.stats["KeyTemperatureK"] / 10;
   ekey.stats["KeyTemperatureC"] = ekey.stats["KeyTemperatureK"] - 273.15;
   ekey.stats["KeyTemperatureF"] = (ekey.stats["KeyTemperatureK"] * 1.8) - 459.67;
   ekey.stats["KeyVoltage"] = ekey.stats["KeyVoltage"] / 1000;

   ekey.stats["KeyRawShannonPerByteL"] = ekey.stats["KeyRawShannonPerByteL"] / 100;
   ekey.stats["KeyRawShannonPerByteR"] = ekey.stats["KeyRawShannonPerByteR"] / 100;
   ekey.stats["KeyRawShannonPerByteX"] = ekey.stats["KeyRawShannonPerByteX"] / 100;
   ekey.stats["KeyDbsdShannonPerByteL"] = ekey.stats["KeyDbsdShannonPerByteL"] / 100;
   ekey.stats["KeyDbsdShannonPerByteR"] = ekey.stats["KeyDbsdShannonPerByteR"] / 100;

   ekey.stats["FipsFrameRate"] = ekey.stats["FipsFrameRate"] / 100;

   ekey.stats["ReadRate"] = math.floor((ekey.stats["BytesRead"] * 8) / ekey.stats["ConnectionTime"])

   ekey.stats["WriteRate"] = math.floor((ekey.stats["BytesWritten"] * 8) / ekey.stats["ConnectionTime"])

   ekey.stats["KeyShortBadness"] = failmodes[ekey.stats["KeyRawBadness"]][1]
   ekey.stats["KeyEnglishBadness"] = failmodes[ekey.stats["KeyRawBadness"]][2]

end

local function update_ekey(ekey)
   ekey.okay, ekey.status, ekey.serial = ekey_query(ekey.ekey)
end

function add_ekey(path, serial)
   assert(output_configured, "No output type yet configured")
   local ekey = find_ekey(path)
   if ekey then
      -- Already got it
      return ekey
   end
   ekey = { nr = ekey_nr, devpath = path, ekey = assert(ekey_add(path, serial)), stats = { } }
   update_ekey(ekey)
   table.insert(ekey_list, ekey)
   ekey_list[ekey] = #ekey_list
   ekey_list[ekey.ekey] = ekey
   ekey_nr = ekey_nr + 1
   return ekey
end

function find_ekey(tag)
   tag = tostring(tag)
   debugprint("Looking for " .. tag)
   for _, ekey in ipairs(ekey_list) do
      if type(ekey) ~= "table" then
	 debugprint("Odd",_,"is",tostring(ekey))
      else
	 update_ekey(ekey)
	 debugprint("Checking ekey from ", tostring(ekey.devpath), "serial", tostring(ekey.serial),"nr",tostring(ekey.nr), "suffix",string.sub(tostring(ekey.devpath),-#tag, -1))
	 if (tostring(ekey.devpath) == tag) or
	    (tostring(ekey.serial) == tag) or
	    (tostring(ekey.nr) == tag) or 
	    (string.sub(tostring(ekey.devpath),-(#tag + 1),-1) == "/"..tag) then
	    return ekey, _
	 end
      end
   end
end

function kill_ekey(ekey)
   local new_tab = {}
   for _, nekey in ipairs(ekey_list) do
      if ekey ~= nekey then
	 new_tab[#new_tab + 1] = nekey
	 new_tab[nekey] = #new_tab
	 new_tab[nekey.ekey] = nekey
      end
   end
   ekey_del(ekey.ekey)
   ekey_list = new_tab
end

-- EGD and other entropy-over-socket type protocols
local entropy_blocks = {}
local total_entropy = 0
local MAX_ENTROPY = 1024*1024

local strsub = string.sub
local strbyte = string.byte
local strchar = string.char
local tremove = table.remove

local function enqueue_entropy(bytes)
   if ((total_entropy + #bytes) <= MAX_ENTROPY) then
      entropy_blocks[#entropy_blocks + 1] = bytes
      total_entropy = total_entropy + #bytes
      --debugprint("Total entropy now " .. tostring(total_entropy) .. " bytes.")
   end
end

local function dequeue_entropy(nbytes)
   local retstr = ""
   while total_entropy > 0 and nbytes > 0 do
      local eval = entropy_blocks[#entropy_blocks]
      if #eval <= nbytes then
	 retstr = retstr .. eval
	 nbytes = nbytes - #eval
	 tremove(entropy_blocks, #entropy_blocks)
	 total_entropy = total_entropy - #eval
      else
	 retstr = retstr .. strsub(eval, 1, nbytes)
	 total_entropy = total_entropy - nbytes
	 entropy_blocks[#entropy_blocks] = strsub(eval, nbytes + 1)
	 nbytes = 0
      end
   end
   return retstr
end

local weakkeyed = { __mode = "k" }
local readbuffers = setmetatable({}, weakkeyed)
local egdwriteblocked = setmetatable({}, weakkeyed)
local egdreadblocked = setmetatable({}, weakkeyed)

local function egd_getbytes(sock, nr)
   local r = readbuffers[sock] or ""
   if #r < nr then return end
   local t = {}
   for i = 1, nr do
      t[i] = strbyte(r, i)
   end
   readbuffers[sock] = strsub(r, nr + 1)
   return unpack(t)
end

local function egd_pushbackread(sock, bval)
   readbuffers[sock] = strchar(bval) .. (readbuffers[sock] or "")
end

local EGD_CMD_QUERYPOOL = 0
local EGD_CMD_READBYTES = 1
local EGD_CMD_BLOCKREAD = 2
local EGD_CMD_ADDENTROPY = 3
local EGD_CMD_GETPID = 4

local function egd_pack32(val)
   local r
   r = strchar(math.mod(val, 256))
   val = math.floor(val / 256)
   r = strchar(math.mod(val, 256)) .. r
   val = math.floor(val / 256)
   r = strchar(math.mod(val, 256)) .. r
   val = math.floor(val / 256)
   r = strchar(math.mod(val, 256)) .. r
   return r
end

local function egd_try_cmd(sock)
   -- If blocked writing entropy, do not process commands
   if egdwriteblocked[sock] then
      --debugprint("Not servicing, write blocked")
      return
   end
   -- If blocked reading stuff to discard, do that first
   if egdreadblocked[sock] then
      if (egd_getbytes(sock, egdreadblocked[sock])) then
	 --debugprint("Read unblocked")
	 egdreadblocked[sock] = nil
      else
	 -- Incomplete, do not attempt command processing
	 --debugprint("Not servicing, read blocked")
	 return
      end
   end
   -- Okay, try and process a command
   local cmd = egd_getbytes(sock, 1)
   -- Just in case there's no command...
   if cmd == nil then return end
   --debugprint("Command: " .. tostring(cmd))
   if cmd == EGD_CMD_QUERYPOOL then
      -- Query pool size, no arguments
      -- Return: U32 of available entropy in bits
      ctlwrite(sock, egd_pack32(total_entropy * 8))
      return
   end
   if cmd == EGD_CMD_READBYTES then
      -- Retrieve N bytes (nonblocking)
      -- Arguments: U8 nbytes
      -- Return: U8 rbytes, STR bytes
      local nbytes = egd_getbytes(sock, 1)
      if nbytes == nil then
	 --debugprint("No request provided, pushing back")
	 egd_pushbackread(sock, EGD_CMD_READBYTES)
	 return
      end
      --debugprint("Dequeuing " .. tostring(nbytes))
      local ent = dequeue_entropy(nbytes)
      --debugprint("Acquired " .. tostring(#ent))
      ctlwrite(sock, strchar(#ent) .. ent)
      return
   end
   if cmd == EGD_CMD_BLOCKREAD then
      -- Retrieve N bytes (blocking)
      -- Arguments: U8 nbytes
      -- Return: STR bytes
      local nbytes = egd_getbytes(sock, 1)
      if nbytes == nil then
	 egd_pushbackread(sock, EGD_CMD_BLOCKREAD)
	 return
      end
      local ent = dequeue_entropy(nbytes)
      ctlwrite(sock, ent)
      if #ent < nbytes then
	 egdwriteblocked[sock] = nbytes - #ent
      end
      return
   end
   if cmd == EGD_CMD_ADDENTROPY then
      -- Add entropy
      -- Arguments: U16 shannons, U8 bytes, STR
      -- Return: None
      local foo, bar, nbytes = egd_getbytes(sock, 3)
      if foo == nil then
	 egd_pushbackread(sock, EGD_CMD_ADDENTROPY)
	 return
      end
      --debugprint("Expect " .. tostring(nbytes))
      if (egd_getbytes(sock, nbytes) == nil) then
	 egdreadblocked[sock] = nbytes
      end
      return
   end
   if cmd == EGD_CMD_GETPID then
      -- Get PID
      -- Arguments: None
      -- Return U8 slen, STR pidstr
      ctlwrite(sock, strchar(2) .. "-1")
      return
   end
   -- Unknown command
   debugprint("Unknown EGD command: " .. tostring(cmd))
   ctltokill[sock] = true
end

local function egd_spreadwrite()
   -- If there are any EGD sockets blocked on entropy, try feeding it around.
   if next(egdwriteblocked) == nil then return end
   debugprint("Doing a spreadwrite")
   local totpending = 0
   for k, v in pairs(egdwriteblocked) do totpending = totpending + v end
   if (totpending > MAX_ENTROPY) then
      debugprint("Somehow, we are blocking for more entropy than we can ever have, skipping the test.")
   else
      if (totpending > total_entropy) then 
	 debugprint("Not enough entropy to satisfy spread-write. Want "..tostring(totpending).." but only have "..tostring(total_entropy))
	 return 
      end
   end

   local to_try = {} -- Filled out with sockets we unblocked, so we can try a command.
   for sock, amount in pairs(egdwriteblocked) do
      local towrite = amount
      debugprint("egd_spreadwrite("..tostring(sock)..") of " .. tostring(towrite))
      local got = dequeue_entropy(towrite)
      if #got > 0 then
	 ctlwrite(sock, got)
	 if towrite == amount then
	    debugprint("huzzah, unblocked")
	    egdwriteblocked[sock] = nil
	    to_try[sock] = true
	 else
	    debugprint("boo hiss, "..tostring(amount-entper).." left")
	    egdwriteblocked[sock] = amount - entper
	 end
      end
   end

   for sock in pairs(to_try) do
      egd_try_cmd(sock)
   end
end

local function egd_ctlread(sock)
   if ctltoaccept[sock] then
      -- Incoming EGD connection, prepare a client connection
      local client = sock:accept()
      client:settimeout(0.010)
      pcall(function() client:setoption("tcp-nodelay", true) end)
      local peer, maybeport = "UNIX", 0
      pcall(function() peer, maybeport = client:getpeername() end)
      if maybeport then peer = peer .. ":" .. tostring(maybeport) end
      local name = "EGDC:" .. peer
      debugprint("New client: " .. name)
      addctlsocket(client, name, true, egd_ctlread)
      readbuffers[client] = ""
      return
   end
   --debugprint("EGD command RX")
   local r,msg = sock:receive(1)
   if r == nil or r == 0 then
      debugprint("Killing")
      ctltokill[sock] = true
      return
   end
   --debugprint("Adding " .. tostring(#r) .. " bytes")
   readbuffers[sock] = (readbuffers[sock] or "") .. r
   --debugprint("Trying...")
   repeat
      egd_try_cmd(sock)
      r, msg = sock:receive(1)
      if r ~= nil then
	 readbuffers[sock] = (readbuffers[sock] or "") .. r
      end
   until msg == "timeout"
end

-- The routines provided to the controlled environment

local currentclient = nil
local gatheredoutput = ""
local gatheredlines = {}

function _(f)
   -- Helper function to force the function environment.
   setfenv(_G[f], _G);
   protectedenv[f] = _G[f]
end

local output_is_folded = false
local function SetFoldedOutput()
   if output_is_folded then return end
   assert(not output_configured, "Output already configured")
   assert(open_foldback_output())
   output_configured = true
   output_is_folded = true
end

if have_unix_domain_sockets then
   function UnixControlSocket(sockname)
      -- Add a UDS control socket to the set of control sockets available
      -- First, try and connect, so we can abort if it's present.
      if socket.unix():connect(sockname) then
	 error("Control socket " .. sockname .. " already present. Is ekeyd already running?")
      end
      -- Okay, clean up (ignoring errors) and create a fresh socket
      unlink(sockname)
      local u = socket.unix()
      assert(u:bind(sockname))
      assert(u:listen())
      addctlsocket(u, "U:" .. sockname)
   end _ "UnixControlSocket"
else
   function UnixControlSocket()
      error("UNIX Domain sockets not supported by LuaSocket")
   end _ "UnixControlSocket"
end

function TCPControlSocket(port)
   -- Add a TCP control socket to the set of control sockets available
   if socket.tcp():connect("127.0.0.1", tonumber(port)) then
      error("TCP Control socket on port " .. tostring(port) .. " already present. Is ekeyd already running?")
   end
   local t = socket.tcp()
   t:setoption("reuseaddr", true)
   assert(t:bind("127.0.0.1", tonumber(port)))
   assert(t:listen())
   addctlsocket(t, "T:" .. tostring(port))
end _ "TCPControlSocket"

if have_unix_domain_sockets then
   function EGDUnixSocket(sockname, modestr, user, group)
      SetFoldedOutput()
      if socket.unix():connect(sockname) then
	 error("EGD socket " .. sockname .. " already present. Is ekeyd/EGD already running?")
      end
      -- Add a UDS control socket to the set of control sockets available
      unlink(sockname)
      local u = socket.unix()
      assert(u:bind(sockname))
      assert(u:listen())
      addctlsocket(u, "U:" .. sockname, false, egd_ctlread)
      if modestr then
	 assert(chmod(sockname, tonumber(modestr, 8)))
	 user = user or ""
	 group = group or ""
	 assert(chown(sockname, user, group))
      else
	 assert(chmod(sockname, tonumber("0600", 8)))
      end
   end _ "EGDUnixSocket"
else
   function EGDUnixSocket()
      error("UNIX Domain sockets not supported by LuaSocket")
   end _ "EGDUnixSocket"
end

function EGDTCPSocket(port, ipaddr)
   SetFoldedOutput()
   ipaddr = ipaddr or "127.0.0.1"
   if socket.tcp():connect(ipaddr, tonumber(port)) then
      error("EGD TCP socket on " .. ipaddr .. ":" .. tostring(port) .. " already present. Is ekeyd/EGD already running?")
   end
   -- Add a TCP control socket to the set of control sockets available
   local t = socket.tcp()
   t:setoption("reuseaddr", true)
   assert(t:bind(ipaddr, tonumber(port)))
   assert(t:listen())
   addctlsocket(t, "T:" .. tostring(port), false, egd_ctlread)
end _ "EGDTCPSocket"

function Bye()
   Print "Good bye"
   ctldielater[currentclient] = true
end _ "Bye"

function MLPrint(...)
   local t = {...}
   for i, v in ipairs(t) do
      if type(v) ~= "string" then
	 t[i] = tostring(v)
      end
   end
   gatheredlines[#gatheredlines+1] = table.concat(t, "\t")
end _ "MLPrint"

function Print(...)
   local t = {...}
   for i, v in ipairs(t) do
      if type(v) ~= "string" then
	 t[i] = tostring(v)
      end
   end
   gatheredoutput = (gatheredoutput and gatheredoutput .. ", " or "") .. table.concat(t, "\t")
end _ "Print"

function KVPrint(key, value)
  gatheredlines[#gatheredlines+1] = key .. "=" .. value 
end _ "KVPrint"


function AddEntropyKey(devpath, serial)
   local mykey = add_ekey(devpath, serial)
   debugprint("Added ekey for " .. tostring(devpath))
   Print("ID "..tostring(mykey.nr))
end _ "AddEntropyKey"

function RemoveEntropyKey(tag)
   local ekey = assert(find_ekey(tag), "Unable to find ekey '" .. tostring(tag) .. "'")
   debugprint("Killing ekey for " .. tostring(tag))
   kill_ekey(ekey)
end _ "RemoveEntropyKey"

function AddEntropyKeys(dirname)
   local function do_it()
      for _, knode in ipairs(assert(enumerate(dirname))) do
	 if string.sub(knode, 1, 1) ~= "." then
	    AddEntropyKey(dirname .. "/" .. knode)
	 end
      end
   end
   pcall(do_it)
end _ "AddEntropyKeys"

function Keyring(fname)
   Print(tostring(read_keys(tostring(fname))))
end _ "Keyring"

function SetOutputToFile(fname)
   assert(not output_configured, "Output already configured")
   assert(open_output_file(fname))
   output_configured = true
end _ "SetOutputToFile"

function SetOutputToKernel(bpb)
   assert(not output_configured, "Output already configured")
   assert(open_kernel_output(tonumber(bpb)))
   output_configured = true
end _ "SetOutputToKernel"

function ListEntropyKeys()
   MLPrint("NR", "OK", "Status", "Path", "SerialNo")
   for _, ekey in ipairs(ekey_list) do
      update_ekey(ekey)
      MLPrint(ekey.nr, ekey.okay and "YES" or "NO", ekey.status or "", ekey.devpath, ekey.serial)
   end
   return tostring(#ekey_list)
end _ "ListEntropyKeys"

function StatEntropyKey(tag)
   local ekey = assert(find_ekey(tag), "Unable to find ekey '" .. tostring(tag) .. "'")

   update_ekey_stat(ekey)

   for i, v in pairs(ekey.stats) do
      KVPrint(i, v)      
   end

end _ "StatEntropyKey"

function Shutdown()
   while #ekey_list > 0 do
      kill_ekey(ekey_list[1])
   end
   local k = next(controlsockets)
   while k do
      delctlsocket(k)
      k = next(controlsockets)
   end
end _ "Shutdown"

function Daemonise(val)
   daemonise(val)
end _ "Daemonise"

-- Routines to run stuff in the controlled environment

local function protected_closure(client, func)
   return function()
	     currentclient = client
	     gatheredoutput = nil
	     gatheredlines = {}
	     local blah = func()
	     if type(blah) == "function" then
		blah()
	     end
	     currentclient = nil
	     return gatheredoutput
	  end
end

function runprotectedcommand(client, cmd)
   dos_callcount = 0
   local func, msg = loadstring("return " .. cmd)
   if not func then
      ctlwrite(client, "ERROR " .. msg .. "\n")
      return
   end
   setfenv(func, ctlenv[client])
   dos_callcount = 0
   local ok, msg = xpcall(protected_closure(client, func), function(m) return tostring(m) end)
   if ok then
      for _, line in ipairs(gatheredlines) do
	 ctlwrite(client, "*\t" .. line .. "\n")
      end
      ctlwrite(client, "OK" .. (msg and " "..msg or "") .. "\n")
   else
      ctlwrite(client, "ERROR " .. msg .. "\n")
   end
end

-- Our interface as-called-from-C.

function FINAL()
   -- Finalise any open FDs, deregister and close all sockets.
   -- Remove all ekeys and output modes, etc.
   dos_callcount = 0
   local killme
   repeat
      killme = next(controlsockets, nil)
      if killme then
	 delctlsocket(killme)
      end
   until killme == nil
end

function CONFIG(cfile)
   -- Prepare a controlled state and run the contents of cfile in it.
   dos_callcount = 0
   local func = assert(loadfile(cfile))
   setfenv(func, protectedenv)
   assert(pcall(func))
   assert(next(controlsockets), "No control interface specified")
   protectedenv.Daemonise = nil
end

function CONTROL()
   dos_callcount = 0
   -- Process anything pending on any of the control sockets.
   local readt, writet = {}, {}

   for sock in pairs(controlsockets) do
      if type(sock) ~= "string" then
	 readt[#readt+1] = sock
      end
   end

   for sock in pairs(ctlwritebuffers) do
      writet[#writet+1] = sock
   end

   readt, writet = socket.select(readt, writet, 0.01)

   for _, reader in ipairs(readt) do
      _ctldoread(reader)
   end

   for _, writer in ipairs(writet) do
      _ctldowrite(writer)
   end

   for sock, cmd in pairs(ctlpendingcmd) do
      if not ctltokill[sock] then
	 runprotectedcommand(sock, cmd)
      end
   end

   -- We've processed them all
   ctlpendingcmd = {}

   local killme
   repeat
      killme = next(ctltokill, nil)
      if killme then
	 delctlsocket(killme)
      end
   until killme == nil

   debugprint("After Control: " .. tostring(math.floor(gc "count")) .. " KiB in use")
end

function INFORM(ekey)
   update_ekey(ekey_list[ekey])
   debugprint("After inform: " .. tostring(math.floor(gc "count")) .. " KiB in use")
end

local lastused = 0
local lastent = 0
function ENTROPY(bytes)
   dos_callcount = 0
   enqueue_entropy(bytes)
   egd_spreadwrite()
   if (debugprint == print) then
      local thisused = math.floor(gc "count")
      local thisent = math.floor(total_entropy/1024)
      if (thisused ~= lastused or thisent ~= lastent) then
	 debugprint("Currently we have: " .. tostring(thisused) .. " KiB in use vs. " .. tostring(thisent) .. " KiB of stored entropy")
	 lastused = thisused
	 lastent = thisent
      end
   end
end

-- Set everything up...

local function hookfunc()
   dos_callcount = dos_callcount + 1
   if dos_callcount > 100 then
      debugprint("D.O.S. hook fired!")
      error("Operation took too long. Stopping.")
   end
end

debug.sethook(hookfunc, "", 100)

