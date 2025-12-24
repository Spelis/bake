---@class WhiskResult
---@field return_code integer
---@field output string
---@field err fun(do_exit:boolean):void

---@type fun(cmd:string):WhiskResult
whisk = whisk

---@type fun(tbl:table):void
bake = bake

---@type fun(name:string, deps:table, fn:function):void
recipe = recipe

---@type fun(msg:string):void
yell = yell
print = print

---@class Pantry
---@field create fun(path:string):void
---@field trash fun(path:string):void
---@field new_shelf fun(path:string):void
---@field is_shelf fun(path:string):boolean
---@field collect fun(dir:string, pattern?:string):table
---@field objects fun(tbl:table, src_pre:string, dst_pre:string, old_ext:string, new_ext:string):table
---@operator fun(dir:string):table
pantry = pantry
