local bulletin_path = "/home/andrew/MagickaBBS/ansis";


function file_exists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
end

bbs_write_string("\r\n\r\nDo you want a quick login? (Y/N) : ");

local char = bbs_read_char();

local i = 0;

if char == "Y" or char == "y" then
	return;
end

-- Display Bulletins
while(true) do
	if file_exists(bulletin_path .. "/bulletin" .. string.format("%d", i) .. ".ans") then
		bbs_write_string("\027[2J\027[1;1H");
		bbs_display_ansi_pause("bulletin" .. string.format("%d", i));
		bbs_write_string("\027[1;37mPress any key to continue...\027[0m");
		bbs_read_char();
	else
		break;
	end

	i = i + 1;
end

-- Display Info
local bbsname;
local sysopname;
local systemname;
local machinename;

bbsname, sysopname, systemname, machinename = bbs_get_info();

bbs_write_string("\r\n\r\n\027[1;37mSystem Information\r\n");
bbs_write_string("\027[1;30m----------------------------------------------\r\n");
bbs_write_string("\027[1;32mBBS Name    : \027[1;37m" .. bbsname .. "\r\n");
bbs_write_string("\027[1;32mSysOp Name  : \027[1;37m" .. sysopname .. "\r\n");
bbs_write_string("\027[1;32mNode        : \027[1;37m" .. string.format("%d", bbs_node()) .. "\r\n");
bbs_write_string("\027[1;32mBBS Version : \027[1;37m" .. bbs_version() .. "\r\n");
bbs_write_string("\027[1;32mSystem      : \027[1;37m" .. systemname .. " (" .. machinename .. ")\r\n");
bbs_write_string("\027[1;30m----------------------------------------------\r\n");
bbs_write_string("\027[1;37mPress any key to continue...\027[0m");
bbs_read_char();

-- Display Last 10 Callers
i = 0;

local user;
local location;
local ltime;

bbs_write_string("\r\n\r\n\027[1;37mLast 10 callers:\r\n");
bbs_write_string("\027[1;30m-------------------------------------------------------------------------------\r\n");

while (i < 10) do
	user, location, ltime = bbs_read_last10(i);
	if (user ~= nil) then
		bbs_write_string(string.format("\027[1;37m%-16s \027[1;36m%-32s \027[1;32m%s\r\n", user, location, os.date("%H:%M %d-%m-%y" ,ltime)));
	end

	i = i + 1;
end
bbs_write_string("\027[1;30m-------------------------------------------------------------------------------\r\n");
bbs_write_string("\027[1;37mPress any key to continue...\027[0m");
bbs_read_char();

-- Check email

local email = bbs_get_emailcount();

if (email > 0) then
	bbs_write_string(string.format("\r\n\r\nYou have %d emails in your inbox\r\n", email));
else
	bbs_write_string("\r\n\r\nYou have no email\r\n");
end

bbs_mail_scan();

bbs_file_scan();

-- Display Auto Message

bbs_display_automsg();

-- Done!
