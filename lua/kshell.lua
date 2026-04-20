-- vim: ts=8 sw=8 noet:
-- NOTE: this function is AI-generated
--
--- Splits a string into an array of tokens, respecting quoted spans and backslash escapes.
--
-- Rules:
--   • Whitespace outside quotes is a delimiter (runs of whitespace are collapsed).
--   • Single-quote  '…'  or double-quote  "…"  spans may contain spaces.
--   • Inside a quoted span the same quote character may be escaped with a
--     backslash:  \"  inside "…"  or  \'  inside '…'.
--   • A backslash before any other character is kept as a literal backslash
--     followed by that character (no special meaning).
--   • An unclosed quote causes the function to return nil plus an error message
--     that includes the column where the quote was opened.
--
-- @param  s  string   The input string to split.
-- @return tokens      table (array) of strings on success.
-- @return nil, err    nil + error string if an unclosed quote is detected.
--
local function split_args(s)
	local tokens = {}
	local token = {} -- buffer for the current token (array of chars)
	local i = 1
	local n = #s
	local in_token = false -- are we currently building a token?

	while i <= n do
		local c = s:sub(i, i)

		-- ── outside any quoted span ──────────────────────────────────────────
		if c == " " or c == "\t" or c == "\n" or c == "\r" then
			-- whitespace: flush the current token (if any) and skip
			if in_token then
				tokens[#tokens + 1] = table.concat(token)
				token = {}
				in_token = false
			end
			i = i + 1
		elseif c == '"' or c == "'" then
			-- opening quote: remember position for error reporting
			local quote = c
			local quote_col = i
			in_token = true
			i = i + 1 -- step past the opening quote

			-- ── inside quoted span ──────────────────────────────────────────
			local closed = false
			while i <= n do
				local qc = s:sub(i, i)

				if qc == "\\" then
					-- backslash escape: peek at next character
					local next = s:sub(i + 1, i + 1)
					if next == quote then
						-- escaped quote -> literal quote character
						token[#token + 1] = quote
						i = i + 2
					else
						-- not escaping the surrounding quote -> keep both chars
						token[#token + 1] = "\\"
						if next ~= "" then
							token[#token + 1] = next
							i = i + 2
						else
							i = i + 1
						end
					end
				elseif qc == quote then
					-- closing quote
					closed = true
					i = i + 1
					break
				else
					token[#token + 1] = qc
					i = i + 1
				end
			end

			if not closed then
				return nil,
					string.format("unfinished quoted string (opened at column %d)", quote_col)
			end
		elseif c == "\\" then
			-- backslash outside quotes: keep both the backslash and next char
			in_token = true
			token[#token + 1] = "\\"
			local next = s:sub(i + 1, i + 1)
			if next ~= "" then
				token[#token + 1] = next
				i = i + 2
			else
				i = i + 1
			end
		else
			-- ordinary character
			in_token = true
			token[#token + 1] = c
			i = i + 1
		end
	end

	-- flush any remaining token
	if in_token then
		tokens[#tokens + 1] = table.concat(token)
	end

	return tokens
end

-- @param  args  string   Table containing the command and its arguments.
-- @return nil, err       nil + error string if an unclosed quote is detected.
local function do_cmd_args(args)
	if type(K) ~= "table" then
		return nil, string.format("K is expected to be table, but actually is %s", type(K))
	end
	if type(K.cmds) ~= "table" then
		return nil, string.format("K.cmds is expected to be table, but actually is %s", type(K))
	end

	if type(args) ~= "table" then
		return nil, string.format("argument 1 - expected table, got %s", type(args))
	end
	if #args == 0 then
		return
	end

	local cmd_name = args[1]
	if cmd_name == "help" then
		print("This is kshell. It is running in kernel mode.")
		print("Enter one of the commands. The spaces in arguments can be quoted.")
		print("Available commands:")
		for name, cmd_obj in pairs(K.cmds) do
			print(string.format("* %s - %s", name, cmd_obj.help))
		end
	else
		local cmd_obj = K.cmds[cmd_name]
		if cmd_obj == nil then
			return nil, string.format("unrecognized command: %s", cmd_name)
		end
		cmd_obj.handler(args)
	end
end

local function do_cmd(cmd)
	local args, err = split_args(cmd)
	if args == nil then
		return nil, err
	end
	return do_cmd_args(args)
end

local function pretty_print_table(t)
	if type(t) ~= "table" then
		print(tostring(t))
		return
	end

	local MAX_KEYS = 16
	local MAX_STR_LEN = 60

	local function format_value(v)
		local tv = type(v)
		if tv == "table" then
			return "{...}"
		elseif tv == "string" then
			if #v > MAX_STR_LEN then
				v = v:sub(1, MAX_STR_LEN) .. "..."
			end
			return string.format("%q", v)
		else
			return tostring(v)
		end
	end

	-- Collect and sort keys: numbers first (ascending), then strings (alphabetical)
	local keys = {}
	for k in pairs(t) do
		keys[#keys + 1] = k
	end
	table.sort(keys, function(a, b)
		local ta, tb = type(a), type(b)
		if ta == tb then
			return a < b
		end
		return ta == "number" -- numbers before strings
	end)

	print("{")
	local omitted = math.max(0, #keys - MAX_KEYS)
	for i = 1, math.min(#keys, MAX_KEYS) do
		local k = keys[i]
		local key_str = type(k) == "string" and k or "[" .. tostring(k) .. "]"
		print(string.format("  %s = %s", key_str, format_value(t[k])))
	end
	if omitted > 0 then
		print(string.format("  ... (%d keys omitted)", omitted))
	end
	print("}")
end

local function pretty_print(value)
	local t = type(value)

	if t == "function" then
		print(string.format("function: %p", value))
	elseif t == "string" then
		print(string.format("%q", value))
	elseif t == "table" then
		pretty_print_table(value)
	else
		print(tostring(value))
	end
end

S = {
	split_args = split_args,
	do_cmd_args = do_cmd_args,
	do_cmd = do_cmd,

	pretty_print = pretty_print,
	pretty_print_table = pretty_print_table,
}
