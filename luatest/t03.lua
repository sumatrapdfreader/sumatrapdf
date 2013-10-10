local winapi = require'winapi'
require'winapi.filesystem'

files = winapi.FindFiles("c:\\*")
table.foreach(files, function (i, f) print(f) end)
