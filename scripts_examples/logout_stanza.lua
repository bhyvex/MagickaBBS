function logout() 
	bbs_write_string("\r\n\r\nAre you sure you want to logoff (Y/N) ? ");
	
	cmd = bbs_read_char();
	
	if (cmd == "n" or cmd == "N") then
		return 0;
	end
	
	bbs_display_ansi("goodbye");
	
	return 1;
end
