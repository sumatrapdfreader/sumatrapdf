-- TODO: doesn't work and I don't get why. I'm following the
-- same pattern of declaring things like other files

local winapi = require'winapi'
require'winapi.wintypes'
require'winapi.filesystem'

files = winapi.FindFiles("c:\\*")
table.foreach(files, print)
