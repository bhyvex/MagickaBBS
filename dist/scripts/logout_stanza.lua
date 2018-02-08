function logout() 
	bbs_write_string("\027[2J");
	bbs_display_ansi("goodbye");
	return 1;
end
