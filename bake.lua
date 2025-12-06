local target = "bake"
local ccflags = "-g"
local ldflags = "-g -llua5.3"

-- neat utility functions :P
local src = pantry.collect("src", ".c")
local obj = pantry.objects(src, "src/", "build/", ".c", ".o")

recipe("build/" .. target, obj, function(output, input)
	local obj_str = table.concat(input, " ")
	whisk("gcc " .. obj_str .. " " .. ldflags .. " -o " .. output)
end)

-- "ALWAYS" dependent targets will always get run, as long as they get referenced by something.
recipe("build", { "ALWAYS" }, function()
	pantry.new_shelf("build")
end)

-- Bake supports wildcards, like Make!
recipe("build/%.o", { "src/%.c" }, function(output, input)
	whisk("gcc -c " .. input[1] .. " " .. ccflags .. " -o " .. output)
end)

recipe("clean", { "ALWAYS" }, function()
	whisk("rm -rf build")
end)

bake({ "build", "build/" .. target })
