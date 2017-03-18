#include <sqlite3.h>
#include "magimail.h"
long sq3_utcoffset;

#define MIN(a,b)  ((a)<(b)? (a):(b))

void sq3_init_msg_db(char *dbpath) {
	char *sql1 = "CREATE TABLE IF NOT EXISTS msgs ("
    					"id INTEGER PRIMARY KEY,"
						"sender TEXT COLLATE NOCASE,"
						"recipient TEXT COLLATE NOCASE,"
						"subject TEXT,"
						"oaddress TEXT,"
						"daddress TEXT,"
						"msgid TEXT,"
						"replyid TEXT,"
						"local INTEGER,"
						"sent INTEGER,"
						"private INTEGER,"
						"datewritten INTEGER,"
						"body TEXT);";
	char *sql2 = "CREATE TABLE IF NOT EXISTS msgptrs ("
						"id INTEGER PRIMARY KEY,"
						"msgid INTEGER,"
						"userid INTEGER);";
						
	sqlite3 *db;
	int rc;
	char *errmsg;
	rc = sqlite3_open(dbpath, &db);
	
	sqlite3_busy_timeout(db, 10000);
	
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}
	
	rc = sqlite3_exec(db, sql1, 0, 0, &errmsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(errmsg);
		sqlite3_close(db);
		return;
	}
	rc = sqlite3_exec(db, sql2, 0, 0, &errmsg);
	if (rc != SQLITE_OK) {
		sqlite3_free(errmsg);
		sqlite3_close(db);
		return;
	}

}

bool sq3_beforefunc(void) {
	time_t t1,t2;
	struct tm *tp;	
	t1=time(NULL);
	tp=gmtime(&t1);
	tp->tm_isdst=-1;
	t2=mktime(tp);
	sq3_utcoffset=t2-t1;
	return TRUE;
}

bool sq3_afterfunc(bool success) {
	return TRUE;
}

bool sq3_importfunc(struct MemMessage *mm,struct Area *area) {
	char buffer[1024];
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	char *sql = "INSERT INTO msgs (sender, recipient, subject, oaddress, daddress, msgid, replyid, local, sent, private, datewritten, body) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
	char *sender;
	char *recipient;
	char *subject;
	char *oaddress;
	char *daddress;
	char *msgid;
	char *replyid;
	int local;
	int sent;
	int private;
	time_t datewritten;
	struct TextChunk *chunk;
	uint32_t c,linebegin,linelen;
	char *msgtext;
	char *body;
	
	uint32_t msgsize,msgpos;	
	if (mm->From[0]) {
		sender = strdup(mm->From);
	} else {
		sender = NULL;
	}
	if (mm->To[0]) {
		recipient = strdup(mm->To);
	} else {
		recipient = NULL;
	}
	if (mm->Subject[0]) {
		subject = strdup(mm->Subject);
	} else {
		subject = NULL;
	}
	if(mm->Area[0] == 0) {
		oaddress = (char *)osAlloc(100);
		Print4D(&mm->OrigNode, oaddress);
		daddress = (char *)osAlloc(100);
		Print4D(&mm->DestNode, daddress);
		private = 1;
	} else {
		oaddress = (char *)osAlloc(100);
		Print4D(&mm->Origin4D, oaddress);
		daddress = NULL;
		private = 0;
	}
	datewritten = FidoToTime(mm->DateTime);
	datewritten -= sq3_utcoffset;
	local = 0;
	sent = 0;
	
	msgpos=0;
	msgsize=0;

	replyid = NULL;
	msgid = NULL;

	for(chunk=(struct TextChunk *)mm->TextChunks.First;chunk;chunk=chunk->Next)
		msgsize+=chunk->Length;

	if(msgsize != 0)
	{
		if(!(msgtext=osAlloc(msgsize)))
		{	
			LogWrite(1,SYSTEMERR,"Out of memory");
			return(FALSE);
		}
	}	
	
   /* Separate kludges from text */

	for(chunk=(struct TextChunk *)mm->TextChunks.First;chunk;chunk=chunk->Next)
		for(c=0;c<chunk->Length;)
		{
			linebegin=msgpos;
			
			while(chunk->Data[c]!=13 && c<chunk->Length)
			{ 
				if(chunk->Data[c]!=10)
					msgtext[msgpos++]=chunk->Data[c];
					
				c++;
			}

			if(chunk->Data[c]==13 && c<chunk->Length)
				msgtext[msgpos++]=chunk->Data[c++];

			linelen=msgpos-linebegin;

			if(linelen!=0)
			{
				if(linelen>=7 && strncmp(&msgtext[linebegin],"\x01""MSGID:",7)==0)
				{
					mystrncpy(buffer,&msgtext[linebegin+7],MIN(100,linelen-7));
					stripleadtrail(buffer);
					msgid = strdup(buffer);
					msgpos=linebegin;
				}
				else if(linelen>=7 && strncmp(&msgtext[linebegin],"\x01""REPLY:",7)==0)
				{
					mystrncpy(buffer,&msgtext[linebegin+7],MIN(100,linelen-7));
					stripleadtrail(buffer);
					replyid = strdup(buffer);
					msgpos=linebegin;
            }
            else if(linelen>=7 && strncmp(&msgtext[linebegin],"\x01""FLAGS:",7)==0)
            {
					msgpos=linebegin;
            }
            else if(linelen>=5 && strncmp(&msgtext[linebegin],"\x01""INTL",5)==0)
            {
               /* Remove this kludge */
					msgpos=linebegin;
            }
            else if(linelen>=5 && strncmp(&msgtext[linebegin],"\x01""TOPT",5)==0)
            {
               /* Remove this kludge */
					msgpos=linebegin;
            }
            else if(linelen>=5 && strncmp(&msgtext[linebegin],"\x01""FMPT",5)==0)
            {
               /* Remove this kludge */
					msgpos=linebegin;
            }
            else if(msgtext[linebegin]==1)
            {
				msgpos=linebegin;
			}
		}
	}
	
	if (msgsize == 0) {
		msgtext = "";
		msgpos = 1;
	}
	
	
	body = (char *)osAlloc(msgpos + 1);
	
	memset(body, 0, msgpos+1);
	memcpy(body, msgtext, msgpos);

	if (msgsize) {
		osFree(msgtext);
	}
	
	snprintf(buffer, 1024, "%s.sq3", area->Path);
	sq3_init_msg_db(buffer);
	
	rc = sqlite3_open(buffer, &db);
	sqlite3_busy_timeout(db, 10000);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return FALSE;
	}
	
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return FALSE;
	}
	
	sqlite3_bind_text(res, 1, sender, -1, 0);
	sqlite3_bind_text(res, 2, recipient, -1, 0);
	sqlite3_bind_text(res, 3, subject, -1, 0);
	sqlite3_bind_text(res, 4, oaddress, -1, 0);
	sqlite3_bind_text(res, 5, daddress, -1, 0);
	sqlite3_bind_text(res, 6, msgid, -1, 0);
	sqlite3_bind_text(res, 7, replyid, -1, 0);
	sqlite3_bind_int(res, 8, local);
	sqlite3_bind_int(res, 9, sent);
	sqlite3_bind_int(res, 10, private);
	sqlite3_bind_int(res, 11, datewritten);
	sqlite3_bind_text(res, 12, body, -1, 0);

	sqlite3_step(res);
	sqlite3_finalize(res);
	sqlite3_close(db);
	
	if (sender != NULL) {
		osFree(sender);
	}
	if (recipient != NULL) {
		osFree(recipient);
	}
	if (subject != NULL) {
		osFree(subject);
	}
	if (oaddress != NULL) {
		osFree(oaddress);
	}
	if (daddress != NULL) {
		osFree(daddress);
	}
	
	if (msgid != NULL) {
		osFree(msgid);
	}
	if (replyid != NULL) {
		osFree(replyid);
	}
	osFree(body);
	return TRUE;
}

void sq3_makekludge(struct MemMessage *mm,char *pre,char *data)
{
   char *buf;
	uint32_t len = strlen(data);
	
	if(!(buf=osAlloc(strlen(pre)+len+10))) /* A few bytes extra */
		return;

   strcpy(buf,pre);
   if(len && data) mystrncpy(&buf[strlen(buf)],data,len+1);
   strcat(buf,"\x0d");
   mmAddLine(mm,buf);

	osFree(buf);
}

bool sq3_rescanfunc(struct Area *area,uint32_t max,bool (*handlefunc)(struct MemMessage *mm)) {
	sqlite3 *db;
	sqlite3_stmt *res;
	int rc;
	char *sql = "SELECT id, sender, recipient, subject, oaddress, daddress, msgid, replyid, local, sent, private, datewritten, body FROM msgs ORDER BY datewritten DESC LIMIT %d";
	char buffer[1024];
	struct MemMessage *mm;
	char domain[20];
	struct Node4D n4d;
	char *msgtext;
	snprintf(buffer, 1024, "%s.sq3", area->Path);
	
	sq3_init_msg_db(buffer);
	
	rc = sqlite3_open(buffer, &db);
	sqlite3_busy_timeout(db, 10000);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return FALSE;
	}
	sprintf(buffer, sql, max);
	rc = sqlite3_prepare_v2(db, buffer, -1, &res, 0);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return FALSE;
	}
	
	while (sqlite3_step(res) == SQLITE_ROW && !ctrlc) {
		mm = mmAlloc();
		if (area->AreaType == AREATYPE_NETMAIL) {
			strcpy(mm->Area, "");
			mm->Attr = FLAG_CRASH | FLAG_LOCAL | FLAG_PVT;
		} else {
			strcpy(mm->Area, area->Tagname);
			mm->Attr = FLAG_CRASH | FLAG_LOCAL;
		}
		mm->msgnum = sqlite3_column_int(res, 0);
		if (sqlite3_column_text(res, 1) != NULL) {
			mystrncpy(mm->From, (char *)sqlite3_column_text(res, 1), 36);
		}
		if (sqlite3_column_text(res, 2) != NULL) {
			mystrncpy(mm->To, (char *)sqlite3_column_text(res, 2), 36);
		}
		
		if (sqlite3_column_text(res, 3) != NULL) {
			mystrncpy(mm->Subject, (char *)sqlite3_column_text(res, 3), 72);
		}
		if (sqlite3_column_text(res, 4) != NULL) {
			if (Parse5D((char *)sqlite3_column_text(res, 4), &n4d, domain)) {
				mm->OrigNode.Zone=n4d.Zone;
				mm->OrigNode.Net=n4d.Net;
				mm->OrigNode.Node=n4d.Node;
				mm->OrigNode.Point=n4d.Point;
			}
		}
		if (sqlite3_column_text(res, 5) != NULL) {
			if (Parse5D((char *)sqlite3_column_text(res, 5), &n4d, domain)) {
				mm->DestNode.Zone=n4d.Zone;
				mm->DestNode.Net=n4d.Net;
				mm->DestNode.Node=n4d.Node;
				mm->DestNode.Point=n4d.Point;
			}
		}
		if (sqlite3_column_text(res, 6) != NULL) {
			sq3_makekludge(mm,"\x01" "MSGID: ", (char *)sqlite3_column_text(res, 6));
		}
		if (sqlite3_column_text(res, 7) != NULL) {
			sq3_makekludge(mm, "\x01" "REPLY: ", (char *)sqlite3_column_text(res, 7));
		}
		MakeFidoDate(sqlite3_column_int(res, 11)+sq3_utcoffset,mm->DateTime);
		
		msgtext = strdup((char *)sqlite3_column_text(res, 12));
		
		mm->Cost = 0;
		
		if(area->AreaType == AREATYPE_NETMAIL)
		{
			if(mm->OrigNode.Zone != mm->DestNode.Zone || (config.cfg_Flags & CFG_FORCEINTL))
			{
				sprintf(buffer,"\x01" "INTL %u:%u/%u %u:%u/%u\x0d",
					mm->DestNode.Zone,
					mm->DestNode.Net,
					mm->DestNode.Node,
					mm->OrigNode.Zone,
					mm->OrigNode.Net,
					mm->OrigNode.Node);

				mmAddLine(mm,buffer);
			}

			if(mm->OrigNode.Point)
			{
				sprintf(buffer,"\x01" "FMPT %u\x0d",mm->OrigNode.Point);
				mmAddLine(mm,buffer);
			}

			if(mm->DestNode.Point)
			{
				sprintf(buffer,"\x01" "TOPT %u\x0d",mm->DestNode.Point);
				mmAddLine(mm,buffer);
			}
			sprintf(buffer,"\x01RESCANNED %u:%u/%u.%u\x0d",area->Aka->Node.Zone,
                                                  area->Aka->Node.Net,
                                                  area->Aka->Node.Node,
                                                  area->Aka->Node.Point);
			mmAddLine(mm,buffer);		
		}
	
	   if(msgtext)
	   {
		  /* Extract origin address */

		  if(mm->Area[0])
		  {
			 uint32_t textpos,d;
			 char originbuf[200];
			 struct Node4D n4d;

			 textpos=0;

			 while(msgtext[textpos])
			 {
				d=textpos;

				while(msgtext[d] != 13 && msgtext[d] != 0)
				   d++;

				if(msgtext[d] == 13)
				   d++;

				if(d-textpos > 11 && strncmp(&msgtext[textpos]," * Origin: ",11)==0)
				{
				   mystrncpy(originbuf,&msgtext[textpos],MIN(d-textpos,200));

				   if(ExtractAddress(originbuf,&n4d))
						Copy4D(&mm->Origin4D,&n4d);
				}

				textpos=d;
			 }
		  }

		  if(!(mmAddBuf(&mm->TextChunks,msgtext,strlen(msgtext))))
		  {
			
			 osFree(msgtext);
			 mmFree(mm);
			 continue;
		  }
	   }
	   osFree(msgtext);
	   mm->Flags |= MMFLAG_RESCANNED;
	   if(!(*handlefunc)(mm)) {
			mmFree(mm);
			continue;
		}
		
		mmFree(mm);
	}
	sqlite3_close(db);
	return TRUE;
}

bool sq3_exportfunc(struct Area *area,bool (*handlefunc)(struct MemMessage *mm)) {
		sqlite3 *db;
	sqlite3_stmt *res;
	sqlite3_stmt *res2;
	int rc;
	char *sql = "SELECT id, sender, recipient, subject, oaddress, daddress, msgid, replyid, local, sent, private, datewritten, body FROM msgs WHERE local=1 and sent=0";
	char *sql2 = "UPDATE msgs SET sent=1 WHERE id=?";
	char buffer[1024];
	struct MemMessage *mm;
	char domain[20];
	struct Node4D n4d;
	char *msgtext;
	snprintf(buffer, 1024, "%s.sq3", area->Path);
	
	sq3_init_msg_db(buffer);
	
	rc = sqlite3_open(buffer, &db);
	sqlite3_busy_timeout(db, 10000);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return FALSE;
	}
	
	rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return FALSE;
	}
	
	while (sqlite3_step(res) == SQLITE_ROW && !ctrlc) {
		mm = mmAlloc();
		if (area->AreaType == AREATYPE_NETMAIL) {
			strcpy(mm->Area, "");
			mm->Attr = FLAG_LOCAL | FLAG_PVT;
		} else {
			strcpy(mm->Area, area->Tagname);
			mm->Attr = FLAG_CRASH | FLAG_LOCAL;
		}
		mm->msgnum = sqlite3_column_int(res, 0);
		if (sqlite3_column_text(res, 1) != NULL) {
			mystrncpy(mm->From, (char *)sqlite3_column_text(res, 1), 36);
		}
		if (sqlite3_column_text(res, 2) != NULL) {
			mystrncpy(mm->To, (char *)sqlite3_column_text(res, 2), 36);
		}
		
		if (sqlite3_column_text(res, 3) != NULL) {
			mystrncpy(mm->Subject, (char *)sqlite3_column_text(res, 3), 72);
		}
		if (sqlite3_column_text(res, 4) != NULL) {
			if (Parse5D((char *)sqlite3_column_text(res, 4), &n4d, domain)) {
				mm->OrigNode.Zone=n4d.Zone;
				mm->OrigNode.Net=n4d.Net;
				mm->OrigNode.Node=n4d.Node;
				mm->OrigNode.Point=n4d.Point;
			}
		}
		if (sqlite3_column_text(res, 5) != NULL) {
			if (Parse5D((char *)sqlite3_column_text(res, 5), &n4d, domain)) {
				mm->DestNode.Zone=n4d.Zone;
				mm->DestNode.Net=n4d.Net;
				mm->DestNode.Node=n4d.Node;
				mm->DestNode.Point=n4d.Point;
			}
		}
		if (sqlite3_column_text(res, 6) != NULL) {
			sq3_makekludge(mm,"\x01" "MSGID: ",(char *)sqlite3_column_text(res, 6));
		}
		if (sqlite3_column_text(res, 7) != NULL) {
			sq3_makekludge(mm, "\x01" "REPLY: ", (char *)sqlite3_column_text(res, 7));
		}
		MakeFidoDate(sqlite3_column_int(res, 11)+sq3_utcoffset,mm->DateTime);
		
		msgtext = strdup((char *)sqlite3_column_text(res, 12));
		
		mm->Cost = 0;
		
		if(area->AreaType == AREATYPE_NETMAIL)
		{
			if(mm->OrigNode.Zone != mm->DestNode.Zone || (config.cfg_Flags & CFG_FORCEINTL))
			{
				sprintf(buffer,"\x01" "INTL %u:%u/%u %u:%u/%u\x0d",
					mm->DestNode.Zone,
					mm->DestNode.Net,
					mm->DestNode.Node,
					mm->OrigNode.Zone,
					mm->OrigNode.Net,
					mm->OrigNode.Node);

				mmAddLine(mm,buffer);
			}

			if(mm->OrigNode.Point)
			{
				sprintf(buffer,"\x01" "FMPT %u\x0d",mm->OrigNode.Point);
				mmAddLine(mm,buffer);
			}

			if(mm->DestNode.Point)
			{
				sprintf(buffer,"\x01" "TOPT %u\x0d",mm->DestNode.Point);
				mmAddLine(mm,buffer);
			}
		}

		if(config.cfg_Flags & CFG_ADDTID)
			AddTID(mm);		
	
	   if(msgtext)
	   {
		  /* Extract origin address */

		  if(mm->Area[0])
		  {
			 uint32_t textpos,d;
			 char originbuf[200];
			 struct Node4D n4d;

			 textpos=0;

			 while(msgtext[textpos])
			 {
				d=textpos;

				while(msgtext[d] != 13 && msgtext[d] != 0)
				   d++;

				if(msgtext[d] == 13)
				   d++;

				if(d-textpos > 11 && strncmp(&msgtext[textpos]," * Origin: ",11)==0)
				{
				   mystrncpy(originbuf,&msgtext[textpos],MIN(d-textpos,200));

				   if(ExtractAddress(originbuf,&n4d))
						Copy4D(&mm->Origin4D,&n4d);
				}

				textpos=d;
			 }
		  }

		  if(!(mmAddBuf(&mm->TextChunks,msgtext,strlen(msgtext))))
		  {
			
			 osFree(msgtext);
			 mmFree(mm);
			 continue;
		  }
	   }
	   osFree(msgtext);
	   mm->Flags |= MMFLAG_EXPORTED;
	   if(!(*handlefunc)(mm)) {
			mmFree(mm);
			continue;
		}
		
		scan_total++;
		rc = sqlite3_prepare_v2(db, sql2, -1, &res2, 0);
		if (rc != SQLITE_OK) {
			mmFree(mm);
			continue;
		}
		
		sqlite3_bind_int(res2, 1, mm->msgnum);
		sqlite3_step(res2);
		sqlite3_finalize(res2);
		mmFree(mm);
	}
	sqlite3_close(db);
	return TRUE;
}
