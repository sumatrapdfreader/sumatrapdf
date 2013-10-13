require'setupluafiles'
local winapi = require'winapi'
require'filesystem'

files = winapi.FindFiles("c:\\*")
table.foreach(files, function (i, f) print(f) end)
