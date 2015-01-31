local multimodeHandle = {
	update = function(self) -- Update both buffers
		if type(self) ~= "table" then
			error("[filesystem] handle:update : No self parameter", 2)
            return
		end
        --print("[fs] mmh:update() called, int. read: "..tostring(self.internal_read))
		local ok, data = pcall(self.internal_read, self.fName)
        if ok then
            --print("[fs] read data")
            if type(data) == "string" then
                --print("[fs] data returned was type string")
                local d2 = data
                data = {string.byte(d2, 1, #d2)}
            end
            if type(data) == "table" then
                --print("[fs] data returned was type table")
                self.readBuffer = data
                self.get = 1
                if string.lower(string.sub(self.mode, 1, 1)) == "a" then
                    if #self.writeBuffer == 0 then
                        self.writeBuffer = data
                    else
                        for i=1, #data do
                            table.insert(self.writeBuffer, data[i])
                        end
                    end
                    self.put = #self.writeBuffer+1
                end
                return true
            else
                error("[filesystem] handle:update : Invalid data returned; got data of type "..type(data), 2)
                return
            end
        else
            error("[filesystem] handle:update: internal read function errored: "..data, 2)
            return
        end
        --print("[fs] buffers filled, returning")
	end,
    reset = function(self)
        if type(self) ~= "table" then
			error("[filesystem] handle:update : No self parameter", 2)
            return
		end
        self.writeBuffer = {}
    end,
	flush = function(self)
		if type(self) ~= "table" then
			error("[filesystem] handle:flush : No self parameter", 2)
            return
		end
		return self.internal_write(self.fName, self.writeBuffer)
	end,
	write = function(self, data)
		if type(self) ~= "table" then
			error("[filesystem] handle:write : No self parameter", 2)
            return
		end
		if type(data) == "string" then
			if #self.writeBuffer == 0 then
				self.writeBuffer = {string.byte(data, 1, #data)}
                for i=1, #self.writeBuffer do
                    if not self.writeBuffer[i] then
                        break
                    end
                    if self.writeBuffer[i] == 0x0D then
                        table.remove(self.writeBuffer, i)
                    end
                end
                self.put = #self.writeBuffer+1
			else
				for i=1, #data do
                    local b = string.byte(data, i, i)
                    if b ~= 0x0D then -- Ignore CR control characters (autoconvert CRLF to LF)
                        self.writeBuffer[self.put] = b
                        self.put = self.put+1
                    end
				end
			end
        elseif type(data) == "table" then
            for i=1, #data do
                if type(data[i]) ~= "number" then
                    error("[filesystem] handle:write: invalid data", 2)
                end
                self.writeBuffer[self.put] = math.abs(data[i] % 256)
                self.put = self.put+1
            end
		elseif type(data) == "number" then
			self.writeBuffer[self.put] = math.abs(data % 256)
			self.put = self.put+1
		else
			error("[filesystem] handle:write : invalid data type: "..type(data), 2)
            return
		end
	end,
	writeLine = function(self, str)
		if type(self) ~= "table" then
			error("[filesystem] handle:writeLine : No self parameter", 2)
            return
		end
		if type(str) == "string" then
			return self:write(str..string.char(0x0A))
		else
			error("[filesystem] handle:writeLine : invalid type for arg 1", 2)
            return
		end
	end,
	read = function(self)
		if type(self) ~= "table" then
			error("[filesystem] handle:read : No self parameter", 2)
            return
		end
		local byte = self.readBuffer[self.get]
		if byte then
			self.get = self.get+1
		end
		return byte
	end,
	readLine = function(self)
		if type(self) ~= "table" then
			error("[filesystem] handle:readLine : No self parameter", 2)
            return
		end
		local str = ""
        if (#self.readBuffer - self.get) <= 0 then
            return
        end
		while true do
			local byte = self:read()
			if byte == nil or string.char(byte) == "\n" then
				break
			else
                if byte ~= 0x0D then -- Ignore CR characters
                    str = str..string.char(byte)
                end
			end
		end
        --print("read line from file "..self.fName..", "..(#self.readBuffer - self.get).." bytes left in buffer")
        --print("type of str is "..type(str))
		return str
	end,
	readAll = function(self)
		if type(self) ~= "table" then
			error("[filesystem] handle:readAll : No self parameter", 2)
            return
		end
        if (#self.readBuffer == 0) or ((#self.readBuffer - self.get) <= 0) then
            return ""
        end
		local str = ""
        for i=self.get, #self.readBuffer do
            if self.readBuffer[i] ~= 0x0D then
                str = str..string.char(self.readBuffer[i])
            end
        end
		if string.char(self.readBuffer[#self.readBuffer]) == "\n" then
			str = string.sub(str, 1, #str-1)
		end
		self.get = #self.readBuffer+1
		if str == "" then
			return
		end
		return str
	end,
	seek_put = function(self, mode, offset)
		if type(self) ~= "table" then
			error("[filesystem] handle:seek_put : No self parameter", 2)
            return
		end
		if (mode == "set") or (mode == 1) then
			self.put = offset
		elseif (mode == "cur") or (mode == 2) then
			self.put = self.put + offset
		elseif (mode == "end") or (mode == 3) then
			self.put = #self.writeBuffer+offset
		end
		return self.put
	end,
	seek_get = function(self, mode, offset)
		if type(self) ~= "table" then
			error("[filesystem] handle:seek_get : No self parameter", 2)
            return
		end
		if (mode == "set") or (mode == 1) then
			self.get = offset
		elseif (mode == "cur") or (mode == 2) then
			self.get = self.get + offset
		elseif (mode == "end") or (mode == 3) then
			self.get = #self.writeBuffer+offset
		end
		return self.get
	end,
	initalize = function(self, file, fmode)
		if type(self) ~= "table" then
			error("[filesystem] handle:initalize : No self parameter", 2)
            return
		end
        --print("[fs] mmh:initalize called")
        
        local read_only_props = { -- stuff that you shouldn't mess with
            fName = file,
            internal_read = fs.read
            internal_write = fs.write
            mode = fmode,
            closed = false,
        }
        
		local object = {
			writeBuffer = {},
			readBuffer = {},
			put = 1,
			get = 1,
            isMultimodeHandle = true,
		}
        object.close = function(self)
            if not read_only_props.closed then
                if type(self) ~= "table" then
                    error("[filesystem] handle:close : No self parameter", 2)
                    return
                end
                if not ((#self.writeBuffer == 0) and (string.lower(string.sub(self.mode, 1, 1)) == "r")) then -- Keep people from accidentally blanking files
                    self:flush()
                end
                read_only_props.internal_read = function() end
                read_only_props.internal_write = function() end
                read_only_props.closed = true
            end
        end
        
        --print("[fs] object created")
		setmetatable(object, {
            __index = function(t, k)
                if (read_only_props[k] ~= nil) then
                    return read_only_props[k]
                elseif (self[k] ~= nil) then
                    return self[k]
                else
                    return rawget(object, k)
                end
            end,
            __newindex = function(t, k, v) -- we're not going to protect the functions; fs.makeFileHandle relies on that functionality
                if (read_only_props[k] ~= nil) then
                    return error("[handle] : Cannot modify read-only property", 2)
                else
                    rawset(t, k, v)
                end
            end,
            __metatable = "file handle",
        })
        --print("[fs] mt set")
		object:update() -- Fill the read (and possibly write) buffer(s).
        --print("[fs] filled buffers, returning")
		return object
	end,
}


function makeFileHandle(path, mode)
    --print("[fs] makeFileHandle called")
    local handle = multimodeHandle:initalize(path, mode)
    --print("[fs] handle initalized")
    -- Handle functions to change: write(), writeLine(), read(), readLine(), readAll(), close()
    handle.oldWrite = handle.write
    handle.oldWriteLine = handle.writeLine
    handle.oldRead = handle.read
    handle.oldReadLine = handle.readLine
    handle.oldReadAll = handle.readAll
    handle.oldClose = handle.close
    handle.write = function(...)
        local args = {...}
        if type(args[1]) == "table" and args[1].isMultimodeHandle then
            return handle:oldWrite(args[2])
        else
            return handle:oldWrite(args[1])
        end
    end
    handle.writeLine = function(...)
        local args = {...}
        if type(args[1]) == "table" and args[1].isMultimodeHandle then
            return handle:oldWrite(args[2].."\n")
        else
            return handle:oldWrite(args[1].."\n")
        end
    end
    handle.read = function(...)
        local args = {...}
        if type(args[1]) == "table" and args[1].isMultimodeHandle then
            return handle:oldRead(args[2])
        else
            return handle:oldRead(args[1])
        end
    end
    handle.readLine = function(...)
        local args = {...}
        if type(args[1]) == "table" and args[1].isMultimodeHandle then
            return handle:oldReadLine(args[2])
        else
            return handle:oldReadLine(args[1])
        end
    end
    handle.readAll = function(...)
        local args = {...}
        if type(args[1]) == "table" and args[1].isMultimodeHandle then
            return handle:oldReadAll(args[2])
        else
            return handle:oldReadAll(args[1])
        end
    end
    handle.close = function(...)
        local args = {...}
        if type(args[1]) == "table" and args[1].isMultimodeHandle then
            return handle:oldClose(args[2])
        else
            return handle:oldClose(args[1])
        end
    end
    --print("[fs] functions overwritten, returning")
	return handle
end

fs.open = function(path, mode)
	return makeFileHandle(path, mode)
end

-- standard CC init code:

-- Install lua parts of the os api
function os.version()
    return "CraftOS 1.6"
end

local localEvents = {}

function os.queueEvent( name, ... )
    local params = { ... }
    
    table.insert( localEvents, { name, params } )
end 

function os.pullEventRaw( sFilter )
    if #localEvents > 0 then
        if not sFilter then
            local e = table.remove( localEvents )
            return e[1], unpack(e[2])
        else
            for i=1, #localEvents do
                if localEvents[i][1] == sFilter then
                    local e = table.remove( localEvents, i )
                    return e[1], unpack(e[2])
                end
            end
        end
    end
    while true do
        local e = { coroutine.yield( sFilter ) }
        
        if e[1] == "key" then
            --[=[
                lua_pushstring( proc->state, "key" );
                lua_pushnumber( proc->state, d->key );
                lua_pushboolean( proc->state, d->released );
                lua_pushboolean( proc->state, d->shift );
                lua_pushboolean( proc->state, d->ctrl );
                lua_pushboolean( proc->state, d->alt );
                
                if( d->is_ascii ) {
                    lua_pushboolean( proc->state, d->character );
                    ...
                    
                --
                
                e[1] == "key"
                e[2] == key code
                e[3] ==  key down / up ( if true, then this is a key up event)
                e[4] ==  shift state
                e[5] ==  ctrl state
                e[6] ==  alt state
                e[7] ==  ascii character code (if applicable)
                
            ]=]--
            if e[7] then
                if sFilter == "char" then
                    os.queueEvent( unpack(e) )
                    return "char", e[7]
                else
                    os.queueEvent( "char", e[7] )
                    return unpack(e)
                end
            else
                if sFilter ~= "char" then
                    return unpack(e)
                end -- else if sFilter == "char" then do nothing
            end
        else
            return unpack(e)
        end
    end
end

--[=[

RAW KEYCODE DEFINES:

#define KEY_F1               0xA0
#define KEY_F2               0xA1
#define KEY_F3               0xA2
#define KEY_F4               0xA3
#define KEY_F5               0xA4
#define KEY_F6               0xA5
#define KEY_F7               0xA6
#define KEY_F8               0xA7
#define KEY_F9               0xA8
#define KEY_F10              0xA9
#define KEY_F11              0xAA
#define KEY_F12              0xAB
#define KEY_Tab              0xAC
#define KEY_Bksp             0xAD
#define KEY_Enter            0xAE
#define KEY_Escape           0xAF

#define KEY_CurLeft          0xD6
#define KEY_CurDown          0xD7
#define KEY_CurRight         0xD8
#define KEY_CurUp            0xD9
#define KEY_Lctrl            0xDA
#define KEY_Lshift           0xDB
#define KEY_Lalt             0xDC
#define KEY_Rctrl            0xDD
#define KEY_Rshift           0xDE
#define KEY_Ralt             0xDF

#define KEY_Insert           0xC0
#define KEY_Home             0xC1
#define KEY_PgUp             0xC2
#define KEY_Delete           0xC3
#define KEY_End              0xC4
#define KEY_PgDn             0xC5
#define KEY_CapsLock         0xC6
#define KEY_NumbLock         0xC7
#define KEY_ScrlLock         0xC8

(keypad codes)
#define KEY_FwSlash          0xE0
#define KEY_Asterisk         0xE1
#define KEY_Dash             0xE2
#define KEY_Plus             0xE3
#define KEY_KeypadEnter      0xE4
#define KEY_KeypadPeriod     0xE5
#define KEY_0                0xE6
#define KEY_1                0xE7
#define KEY_2                0xE8
#define KEY_3                0xE9
#define KEY_4                0xEA
#define KEY_5                0xEB
#define KEY_6                0xEC
#define KEY_7                0xED
#define KEY_8                0xEE
#define KEY_9                0xEF

(ACPI / Multimedia codes)
#define KEY_WWWSearch        0x01
#define KEY_PrevTrk          0x02
#define KEY_WWW Fav          0x03
#define KEY_Lgui             0x04
#define KEY_WWWRfsh          0x05
#define KEY_VolDown          0x06
#define KEY_Mute             0x07
#define KEY_Rgui             0x08
#define KEY_WWWStop          0x09
#define KEY_Calc             0x0A
#define KEY_Apps             0x0B
#define KEY_WWWFwd           0x0C
#define KEY_VolUp            0x0D
#define KEY_PlayPause        0x0E
#define KEY_Power            0x0F
#define KEY_WWWBack          0x10
#define KEY_WWWHome          0x11
#define KEY_Stop             0x12
#define KEY_Sleep            0x13
#define KEY_MyComp           0x14
#define KEY_Email            0x15
#define KEY_NextTrk          0x16
#define KEY_MediaSel         0x17
#define KEY_Wake             0x18

--]=]

function os.pullEvent( sFilter )
    local eventData = { os.pullEventRaw( sFilter ) }
    if eventData[1] == "terminate" then
        error( "Terminated", 0 )
    end
    return unpack( eventData )
end

-- Install globals
function sleep( nTime )
    local timer = os.startTimer( nTime or 0 )
    repeat
        local sEvent, param = os.pullEvent( "timer" )
    until param == timer
end

function write( sText )
    local w,h = term.getSize()        
    local x,y = term.getCursorPos()
    
    local nLinesPrinted = 0
    local function newLine()
        if y + 1 <= h then
            term.setCursorPos(1, y + 1)
        else
            term.setCursorPos(1, h)
            term.scroll(1)
        end
        x, y = term.getCursorPos()
        nLinesPrinted = nLinesPrinted + 1
    end
    
    -- Print the line with proper word wrapping
    while string.len(sText) > 0 do
        local whitespace = string.match( sText, "^[ \t]+" )
        if whitespace then
            -- Print whitespace
            term.write( whitespace )
            x,y = term.getCursorPos()
            sText = string.sub( sText, string.len(whitespace) + 1 )
        end
        
        local newline = string.match( sText, "^\n" )
        if newline then
            -- Print newlines
            newLine()
            sText = string.sub( sText, 2 )
        end
        
        local text = string.match( sText, "^[^ \t\n]+" )
        if text then
            sText = string.sub( sText, string.len(text) + 1 )
            if string.len(text) > w then
                -- Print a multiline word                
                while string.len( text ) > 0 do
                    if x > w then
                        newLine()
                    end
                    term.write( text )
                    text = string.sub( text, (w-x) + 2 )
                    x,y = term.getCursorPos()
                end
            else
                -- Print a word normally
                if x + string.len(text) - 1 > w then
                    newLine()
                end
                term.write( text )
                x,y = term.getCursorPos()
            end
        end
    end
    
    return nLinesPrinted
end

function print( ... )
    local nLinesPrinted = 0
    for n,v in ipairs( { ... } ) do
        nLinesPrinted = nLinesPrinted + write( tostring( v ) )
    end
    nLinesPrinted = nLinesPrinted + write( "\n" )
    return nLinesPrinted
end

function printError( ... )
    if term.isColour() then
        term.setTextColour( colours.red )
    end
    print( ... )
    term.setTextColour( colours.white )
end

function read( _sReplaceChar, _tHistory )
    term.setCursorBlink( true )

    local sLine = ""
    local nHistoryPos
    local nPos = 0
    if _sReplaceChar then
        _sReplaceChar = string.sub( _sReplaceChar, 1, 1 )
    end
    
    local w = term.getSize()
    local sx = term.getCursorPos()
    
    local function redraw( _sCustomReplaceChar )
        local nScroll = 0
        if sx + nPos >= w then
            nScroll = (sx + nPos) - w
        end

        local cx,cy = term.getCursorPos()
        term.setCursorPos( sx, cy )
        local sReplace = _sCustomReplaceChar or _sReplaceChar
        if sReplace then
            term.write( string.rep( sReplace, math.max( string.len(sLine) - nScroll, 0 ) ) )
        else
            term.write( string.sub( sLine, nScroll + 1 ) )
        end
        term.setCursorPos( sx + nPos - nScroll, cy )
    end
    
    while true do
        local sEvent, param = os.pullEvent()
        if sEvent == "char" then
            -- Typed key
            sLine = string.sub( sLine, 1, nPos ) .. param .. string.sub( sLine, nPos + 1 )
            nPos = nPos + 1
            redraw()

        elseif sEvent == "paste" then
            -- Pasted text
            sLine = string.sub( sLine, 1, nPos ) .. param .. string.sub( sLine, nPos + 1 )
            nPos = nPos + string.len( param )
            redraw()

        elseif sEvent == "key" then
            if param == keys.enter then
                -- Enter
                break
                
            elseif param == keys.left then
                -- Left
                if nPos > 0 then
                    nPos = nPos - 1
                    redraw()
                end
                
            elseif param == keys.right then
                -- Right                
                if nPos < string.len(sLine) then
                    redraw(" ")
                    nPos = nPos + 1
                    redraw()
                end
            
            elseif param == keys.up or param == keys.down then
                -- Up or down
                if _tHistory then
                    redraw(" ")
                    if param == keys.up then
                        -- Up
                        if nHistoryPos == nil then
                            if #_tHistory > 0 then
                                nHistoryPos = #_tHistory
                            end
                        elseif nHistoryPos > 1 then
                            nHistoryPos = nHistoryPos - 1
                        end
                    else
                        -- Down
                        if nHistoryPos == #_tHistory then
                            nHistoryPos = nil
                        elseif nHistoryPos ~= nil then
                            nHistoryPos = nHistoryPos + 1
                        end                        
                    end
                    if nHistoryPos then
                        sLine = _tHistory[nHistoryPos]
                        nPos = string.len( sLine ) 
                    else
                        sLine = ""
                        nPos = 0
                    end
                    redraw()
                end
            elseif param == keys.backspace then
                -- Backspace
                if nPos > 0 then
                    redraw(" ")
                    sLine = string.sub( sLine, 1, nPos - 1 ) .. string.sub( sLine, nPos + 1 )
                    nPos = nPos - 1                    
                    redraw()
                end
            elseif param == keys.home then
                -- Home
                redraw(" ")
                nPos = 0
                redraw()        
            elseif param == keys.delete then
                -- Delete
                if nPos < string.len(sLine) then
                    redraw(" ")
                    sLine = string.sub( sLine, 1, nPos ) .. string.sub( sLine, nPos + 2 )                
                    redraw()
                end
            elseif param == keys["end"] then
                -- End
                redraw(" ")
                nPos = string.len(sLine)
                redraw()
            end

        elseif sEvent == "term_resize" then
            -- Terminal resized
            w = term.getSize()
            redraw()

        end
    end

    local cx, cy = term.getCursorPos()
    term.setCursorBlink( false )
    term.setCursorPos( w + 1, cy )
    print()
    
    return sLine
end

loadfile = function( _sFile )
    local file = fs.open( _sFile, "r" )
    if file then
        local func, err = loadstring( file.readAll(), fs.getName( _sFile ) )
        file.close()
        return func, err
    end
    return nil, "File not found"
end

dofile = function( _sFile )
    local fnFile, e = loadfile( _sFile )
    if fnFile then
        setfenv( fnFile, getfenv(2) )
        return fnFile()
    else
        error( e, 2 )
    end
end

-- Install the rest of the OS api
function os.run( _tEnv, _sPath, ... )
    local tArgs = { ... }
    local fnFile, err = loadfile( _sPath )
    if fnFile then
        local tEnv = _tEnv
        --setmetatable( tEnv, { __index = function(t,k) return _G[k] end } )
        setmetatable( tEnv, { __index = _G } )
        setfenv( fnFile, tEnv )
        local ok, err = pcall( function()
            fnFile( unpack( tArgs ) )
        end )
        if not ok then
            if err and err ~= "" then
                printError( err )
            end
            return false
        end
        return true
    end
    if err and err ~= "" then
        printError( err )
    end
    return false
end

-- Prevent access to metatables of strings, as these are global between all computers
--[=[
do
    local nativegetfenv = getfenv
    local nativegetmetatable = getmetatable
    local nativeerror = error
    local nativetype = type
    local string_metatable = nativegetfenv(("").gsub)
    function getmetatable( t )
        local mt = nativegetmetatable( t )
        if mt == string_metatable then
            nativeerror( "Attempt to access string metatable", 2 )
        else
            return mt
        end
    end
    function getfenv( env )
        if env == nil then
            env = 2
        elseif nativetype( env ) == "number" and env > 0 then
            env = env + 1
        end
        local fenv = nativegetfenv(env)
        if fenv == string_metatable then
            --nativeerror( "Attempt to access string metatable", 2 )
            return nativegetfenv( 0 )
        else
            return fenv
        end
    end
end
]=]--

local tAPIsLoading = {}
function os.loadAPI( _sPath )
    local sName = fs.getName( _sPath )
    if tAPIsLoading[sName] == true then
        printError( "API "..sName.." is already being loaded" )
        return false
    end
    tAPIsLoading[sName] = true
        
    local tEnv = {}
    setmetatable( tEnv, { __index = _G } )
    local fnAPI, err = loadfile( _sPath )
    if fnAPI then
        setfenv( fnAPI, tEnv )
        fnAPI()
    else
        printError( err )
        tAPIsLoading[sName] = nil
        return false
    end
    
    local tAPI = {}
    for k,v in pairs( tEnv ) do
        tAPI[k] =  v
    end
    
    _G[sName] = tAPI    
    tAPIsLoading[sName] = nil
    return true
end

function os.unloadAPI( _sName )
    if _sName ~= "_G" and type(_G[_sName]) == "table" then
        _G[_sName] = nil
    end
end

function os.sleep( nTime )
    sleep( nTime )
end

--local nativeShutdown = os.shutdown
function os.shutdown()
    --nativeShutdown()
    while true do
        coroutine.yield()
    end
end

--local nativeReboot = os.reboot
function os.reboot()
    --nativeReboot()
    while true do
        coroutine.yield()
    end
end