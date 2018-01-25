<HTML>
<HEAD>
	<TITLE>Magicka BBS</TITLE>
	<link rel="stylesheet" type="text/css" href="@@WWW_URL@@static/style.css">
	<meta charset="utf-8">
	<script language='javascript'>
		function validate () {
			if(document.getElementById('recipient').value=="") {
				alert("To can not be empty.");
				return false;
			}
			if(document.getElementById('subject').value=="") {
				alert("Subject can not be empty");
				return false;
			}
			if(document.getElementById('body').value=="") {
				alert("Can't send an empty message!");
				return false;
			}
			return true;
		}
	</script>	
</HEAD>
<BODY>
	<div class="header">
		<img src="@@WWW_URL@@static/header.png" alt="Magicka BBS" />
	</div>
	<div class="menu">
		<ul>
			<li><a href="@@WWW_URL@@">Home</a></li>
			<li><a href="@@WWW_URL@@last10/">Last 10 Callers</a></li>			
			<li><a href="@@WWW_URL@@email/">Email</a></li>
			<li><a href="@@WWW_URL@@msgs/">Conferences</a></li>
		</ul>
	</div>
	<hr />
	
