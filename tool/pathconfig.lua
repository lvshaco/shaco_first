package.path = package.path .. ";../thirdlib/lua/?.lua"
package.cpath = package.cpath .. ";../thirdlib/lua/?.so"
print(package.path)
print(package.cpath)

