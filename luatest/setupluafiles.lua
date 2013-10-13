-- setup require path to include lua-files project
-- (https://code.google.com/p/lua-files/)
-- we assume it's checked out in ..\.. directory
-- i.e. at the same level as sumatrapdf directory
-- print(package.path)
package.path = package.path .. ";..\\..\\lua-files\\?.lua"
-- print(package.path)
