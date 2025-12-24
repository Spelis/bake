require("bake_stubs")

local target = "bake"
local ccflags = "-g"
local ldflags = "-g -llua5.3"

-- neat utility functions :P
local src = pantry.collect("src", ".c")
local obj = pantry.objects(src, "src/", "build/", ".c", ".o")

recipe("build/" .. target, obj, function(output, input)
	local obj_str = table.concat(input, " ")
	whisk("gcc " .. obj_str .. " " .. ldflags .. " -o " .. output).err(true)
end)

-- "ALWAYS" dependent targets will always get run, as long as they get referenced by something.
recipe("build", { "ALWAYS" }, function()
	pantry.new_shelf("build")
end)

recipe("install", { "ALWAYS" }, function()
	whisk("cp build/" .. target .. " /bin/" .. target).err(false)
	local stub_file = "bake_stubs.lua"

	local first_path = package.path:match("([^;]+)"):gsub("%?%.lua$", "")
	print("Installing stub to: ", first_path)

	if not pantry.is_shelf(first_path) then
		error("First path is not a directory: " .. first_path)
	end

	local copy_stub = whisk("cp " .. stub_file .. " " .. first_path .. "/" .. stub_file)
	copy_stub.err(false)

	if copy_stub.return_code == 0 then
		print("Stub installed successfully!")
	end
end)

-- Bake supports wildcards, like Make!
recipe("build/%.o", { "src/%.c" }, function(output, input)
	local timestr = os.date("%y_%m_%d:%H.%M")
	whisk("gcc -D'VERSION=\"" .. timestr .. "\"' -c " .. input[1] .. " " .. ccflags .. " -o " .. output).err(true)
end)

recipe("clean", { "ALWAYS" }, function()
	whisk("rm -rf build").err(false)
end)

bake({ "build", "build/" .. target })
