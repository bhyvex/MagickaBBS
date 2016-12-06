#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <termios.h>
#include <fcntl.h>
#include "bluewave.h"
#include "jamlib/jam.h"
#include "bbs.h"

extern struct bbs_config conf;
extern struct user_record *gUser;
extern int mynode;
extern char upload_filename[1024];
extern int sshBBS;

tLONG convertl(tLONG l) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	unsigned char result_bytes[4];
	unsigned int result;
	result_bytes[0] = (unsigned char) ((l >> 24) & 0xFF);
	result_bytes[1] = (unsigned char) ((l >> 16) & 0xFF);
	result_bytes[2] = (unsigned char) ((l >> 8) & 0xFF);
	result_bytes[3] = (unsigned char) (l & 0xFF);
	memcpy(&result, result_bytes, 4);
	return result;
#else
	return l;
#endif
}

tWORD converts(tWORD s) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	unsigned char result_bytes[2];
	unsigned short result;
	result_bytes[0] = (unsigned char) ((s >> 8) & 0xFF);
	result_bytes[1] = (unsigned char) (s & 0xFF);
	memcpy(&result, result_bytes, 4);
	return result;
#else
	return s;
#endif
}


int bwave_scan_area(int confr, int area, int areano, int totmsgs, FILE *fti_file, FILE *mix_file, FILE *dat_file, int *last_ptr) {
	struct msg_headers *msghs = read_message_headers(confr, area, gUser);
	int all_unread = 1;
	s_JamBase *jb;
	s_JamLastRead jlr;
	int i;
	int k;
	MIX_REC mix;
	int area_msgs;
	int personal_msgs;
	long mixptr;
	FTI_REC fti;
	struct fido_addr *fido;
	char *body;
	struct tm timeStruct;
	char *month_name[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
		
	if (msghs == NULL) {
		return totmsgs;
	}
	
	jb = open_jam_base(conf.mail_conferences[confr]->mail_areas[area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[confr]->mail_areas[area]->path);
		if (msghs != NULL) {
			free_message_headers(msghs);
		}
		return totmsgs;
	} else {
		all_unread = 0;
		if (JAM_ReadLastRead(jb, gUser->id, &jlr) == JAM_NO_USER) {
			jlr.LastReadMsg = 0;
			jlr.HighReadMsg = 0;
			all_unread = 1;
		}

	}
	
	if (all_unread == 0) {
		k = jlr.HighReadMsg;
		for (i=0;i<msghs->msg_count;i++) {
			if (msghs->msgs[i]->msg_h->MsgNum == k) {
				break;
			}
		}
		i+=1;							
	} else {
		i = 0;
	}
	
	mixptr = ftell(fti_file);
	area_msgs = 0;
	personal_msgs = 0;
	
	for (k=i;k<msghs->msg_count;k++) {
		
		if (totmsgs == conf.bwave_max_msgs) {
			break;
		}
		
		if (strcasecmp(msghs->msgs[k]->to, gUser->loginname) == 0) {
			personal_msgs++;
		}
		
		memset(&fti, 0, sizeof(FTI_REC));
		
		strncpy(fti.from, msghs->msgs[k]->from, 35);
		strncpy(fti.to, msghs->msgs[k]->to, 35);
		strncpy(fti.subject, msghs->msgs[k]->subject, 71);
		
		localtime_r((time_t *)&msghs->msgs[k]->msg_h->DateWritten, &timeStruct);
		
		sprintf(fti.date, "%02d-%s-%04d %02d:%02d", timeStruct.tm_mday, month_name[timeStruct.tm_mon], timeStruct.tm_year + 1900, timeStruct.tm_hour, timeStruct.tm_min);
		fti.msgnum = converts((tWORD)msghs->msgs[k]->msg_h->MsgNum);
		fti.replyto = 0;
		fti.replyat = 0;
		fti.msgptr = convertl(*last_ptr);
		fti.msglength = convertl(msghs->msgs[k]->msg_h->TxtLen);
		
		*last_ptr += msghs->msgs[k]->msg_h->TxtLen;
		
		if (msghs->msgs[k]->msg_h->Attribute & MSG_LOCAL) {
			fti.flags |= FTI_MSGLOCAL;
		}
		
		fti.flags = converts(fti.flags);
		
		fido = parse_fido_addr(msghs->msgs[k]->oaddress);
		if (fido != NULL) {
			fti.orig_zone = converts(fido->zone);
			fti.orig_net = converts(fido->net);
			fti.orig_node = converts(fido->node);
			free(fido);
		} else {
			fti.orig_zone = 0;
			fti.orig_net = 0;
			fti.orig_node = 0;
		}
		// write msg data
		body = (char *)malloc(msghs->msgs[k]->msg_h->TxtLen);
		JAM_ReadMsgText(jb, msghs->msgs[k]->msg_h->TxtOffset, msghs->msgs[k]->msg_h->TxtLen, (char *)body);
		
		fwrite(body, 1, msghs->msgs[k]->msg_h->TxtLen, dat_file);
		fwrite(&fti, sizeof(FTI_REC), 1, fti_file);
		
		free(body);
		jlr.LastReadMsg = msghs->msgs[k]->msg_h->MsgNum;
		if (jlr.HighReadMsg < msghs->msgs[k]->msg_h->MsgNum) {
			jlr.HighReadMsg = msghs->msgs[k]->msg_h->MsgNum;
		}		
		
		JAM_WriteLastRead(jb, gUser->id, &jlr);
		
		area_msgs++;
		totmsgs++;
	}
	
	if (area_msgs) {
	
		memset(&mix, 0, sizeof(MIX_REC));
		
		snprintf(mix.areanum, 6, "%d", areano);
		mix.totmsgs = converts(area_msgs);
		mix.numpers = converts(personal_msgs);
		mix.msghptr = convertl(mixptr);
		fwrite(&mix, sizeof(MIX_REC), 1, mix_file);
	}
	JAM_CloseMB(jb);
	free_message_headers(msghs);
	return totmsgs;
}

void bwave_create_packet() {
	char buffer[1024];
	char archive[1024];
	INF_HEADER hdr;
	INF_AREA_INFO **areas = NULL;
	int i;
	int j;
	int area_count;
	tWORD flags;
	int lasttot;
	int bpos;
	int last_ptr = 0;
	
	struct termios oldit;
	struct termios oldot;
	struct stat s;
	
	FILE *mix_file;
	FILE *fti_file;
	FILE *dat_file;
	FILE *inf_file;
	
	int totmsgs = 0;
	
	area_count = 0;
	
	memset(&hdr, 0, sizeof(INF_HEADER));
	
	hdr.ver = PACKET_LEVEL;
	strncpy(hdr.loginname, gUser->loginname, 42);
	strncpy(hdr.aliasname, gUser->loginname, 42);
	hdr.zone = converts(conf.main_aka->zone);
	hdr.node = converts(conf.main_aka->node);
	hdr.net = converts(conf.main_aka->net);
	hdr.point = converts(conf.main_aka->point);
	strncpy(hdr.sysop, conf.sysop_name, 40);
	
	strncpy(hdr.systemname, conf.bbs_name, 64);
	hdr.inf_header_len = converts(sizeof(INF_HEADER));
	hdr.inf_areainfo_len = converts(sizeof(INF_AREA_INFO));
	hdr.mix_structlen = converts(sizeof(MIX_REC));
	hdr.fti_structlen = converts(sizeof(FTI_REC));
	hdr.uses_upl_file = 1;
	hdr.from_to_len = 35;
	hdr.subject_len = 71;
	memcpy(hdr.packet_id, conf.bwave_name, strlen(conf.bwave_name));
	
	snprintf(buffer, 1024, "%s/node%d", conf.bbs_path, mynode);

	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}

	
	snprintf(buffer, 1024, "%s/node%d/%s.FTI", conf.bbs_path, mynode, conf.bwave_name);
	
	fti_file = fopen(buffer, "wb");
	
	snprintf(buffer, 1024, "%s/node%d/%s.MIX", conf.bbs_path, mynode, conf.bwave_name);
	mix_file = fopen(buffer, "wb");
	
	snprintf(buffer, 1024, "%s/node%d/%s.DAT", conf.bbs_path, mynode, conf.bwave_name);
	dat_file = fopen(buffer, "wb");
	
	s_printf("\r\n");
	
	for (i=0;i<conf.mail_conference_count;i++) {
		for (j=0;j<conf.mail_conferences[i]->mail_area_count;j++) {
			if (conf.mail_conferences[i]->mail_areas[j]->read_sec_level <= gUser->sec_level && conf.mail_conferences[i]->mail_areas[j]->qwkname != NULL) {
				lasttot = totmsgs;
				totmsgs = bwave_scan_area(i, j, area_count+1, totmsgs, fti_file, mix_file, dat_file, &last_ptr);
				s_printf(get_string(195), conf.mail_conferences[i]->name, conf.mail_conferences[i]->mail_areas[j]->name, totmsgs - lasttot); 
				if (lasttot == totmsgs) {
					continue;
				}
				
				if (area_count == 0) {
					areas = (INF_AREA_INFO **)malloc(sizeof(INF_AREA_INFO *));
				} else {
					areas = (INF_AREA_INFO **)realloc(areas, sizeof(INF_AREA_INFO *) * (area_count + 1));
				}
				flags = 0;
				areas[area_count] = (INF_AREA_INFO *)malloc(sizeof(INF_AREA_INFO));
				
				memset(areas[area_count], 0, sizeof(INF_AREA_INFO));
				
				snprintf(areas[area_count]->areanum, 6, "%d", area_count + 1);
				
				memcpy(areas[area_count]->echotag, conf.mail_conferences[i]->mail_areas[j]->qwkname, strlen(conf.mail_conferences[i]->mail_areas[j]->qwkname));
				
				strncpy(areas[area_count]->title, conf.mail_conferences[i]->mail_areas[j]->name, 49);
				
				if (conf.mail_conferences[i]->mail_areas[j]->write_sec_level <= gUser->sec_level) {
					flags |= INF_POST;
				}
				
				if (conf.mail_conferences[i]->mail_areas[j]->type == TYPE_NETMAIL_AREA) {
					flags |= INF_NO_PUBLIC;
					flags |= INF_NETMAIL;
					flags |= INF_ECHO;
				}
				
				if (conf.mail_conferences[i]->mail_areas[j]->type == TYPE_ECHOMAIL_AREA) {
					flags |= INF_NO_PRIVATE;
					flags |= INF_ECHO;
				}
				
				if (conf.mail_conferences[i]->mail_areas[j]->type == TYPE_LOCAL_AREA) {
					flags |= INF_NO_PRIVATE;
				}
				
				flags |= INF_SCANNING;
				
				areas[area_count]->area_flags = converts(flags);
				areas[area_count]->network_type = INF_NET_FIDONET;

				area_count++;
				if (totmsgs == conf.bwave_max_msgs) {
					break;
				}
				
			}
		}
		if (totmsgs == conf.bwave_max_msgs) {
			break;
		}
	}
	
	fclose(dat_file);
	fclose(mix_file);
	fclose(fti_file);
	
	snprintf(buffer, 1024, "%s/node%d/%s.INF", conf.bbs_path, mynode, conf.bwave_name);
	
	inf_file = fopen(buffer, "wb");
	fwrite(&hdr, sizeof(INF_HEADER), 1, inf_file);
	
	for (i=0;i<area_count;i++) {
		fwrite(areas[i], sizeof(INF_AREA_INFO), 1, inf_file);
	}
	
	fclose(inf_file);
	
	for (i=0;i<area_count;i++) {
		free(areas[i]);
	}
	if (areas != NULL) {
		free(areas);
	}
	
	if (totmsgs > 0) {
		// create archive
		bpos = 0;
		snprintf(archive, 1024, "%s/node%d/%s.%03d", conf.bbs_path, mynode, conf.bwave_name, gUser->bwavepktno);
		
		for (i=0;i<strlen(conf.zip_cmd);i++) {
			if (conf.zip_cmd[i] == '*') {
				i++;
				if (conf.zip_cmd[i] == 'a') {
					sprintf(&buffer[bpos], "%s", archive);
					bpos = strlen(buffer);
				} else if (conf.zip_cmd[i] == 'f') {
					sprintf(&buffer[bpos], "%s/node%d/%s.INF %s/node%d/%s.MIX %s/node%d/%s.FTI %s/node%d/%s.DAT", conf.bbs_path, mynode, conf.bwave_name, conf.bbs_path, mynode, conf.bwave_name, conf.bbs_path, mynode, conf.bwave_name, conf.bbs_path, mynode, conf.bwave_name);
					bpos = strlen(buffer);
				} else if (conf.zip_cmd[i] == '*') {
					buffer[bpos++] = '*';
					buffer[bpos] = '\0';
				}
			} else {
				buffer[bpos++] = conf.zip_cmd[i];
				buffer[bpos] = '\0';
			}
		}
		system(buffer);


		if (sshBBS) {
			ttySetRaw(STDIN_FILENO, &oldit);
			ttySetRaw(STDOUT_FILENO, &oldot);
		}
		download_zmodem(gUser, archive);
		if (sshBBS) {
			tcsetattr(STDIN_FILENO, TCSANOW, &oldit);
			tcsetattr(STDOUT_FILENO, TCSANOW, &oldot);
		}
		snprintf(buffer, 1024, "%s/node%d/%s.FTI", conf.bbs_path, mynode, conf.bwave_name);
		unlink(buffer);
		snprintf(buffer, 1024, "%s/node%d/%s.MIX", conf.bbs_path, mynode, conf.bwave_name);
		unlink(buffer);
		snprintf(buffer, 1024, "%s/node%d/%s.DAT", conf.bbs_path, mynode, conf.bwave_name);
		unlink(buffer);
		snprintf(buffer, 1024, "%s/node%d/%s.INF", conf.bbs_path, mynode, conf.bwave_name);
		unlink(buffer);
		unlink(archive);
		gUser->bwavepktno++;
		if (gUser->bwavepktno > 999) {
			gUser->bwavepktno = 0;
		}
		save_user(gUser);
	}
	
	s_printf(get_string(6));
	s_getc();
}


int bwave_add_message(int confr, int area, unsigned int dwritten, char *to, char *subject, struct fido_addr *destaddr, char *msg) {
	s_JamBase *jb;
	s_JamMsgHeader jmh;
	s_JamSubPacket* jsp;
	s_JamSubfield jsf;
	int z;
	char buffer[256];
	
	jb = open_jam_base(conf.mail_conferences[confr]->mail_areas[area]->path);
	if (!jb) {
		dolog("Error opening JAM base.. %s", conf.mail_conferences[confr]->mail_areas[area]->path);
		return 1;
	}

	JAM_ClearMsgHeader( &jmh );
	jmh.DateWritten = dwritten;
	jmh.Attribute |= MSG_LOCAL;
	if (conf.mail_conferences[confr]->realnames == 0) {
		strcpy(buffer, gUser->loginname);
	} else {
		sprintf(buffer, "%s %s", gUser->firstname, gUser->lastname);
	}
	

	jsp = JAM_NewSubPacket();

	jsf.LoID   = JAMSFLD_SENDERNAME;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(buffer);
	jsf.Buffer = (char *)buffer;
	JAM_PutSubfield(jsp, &jsf);

	jsf.LoID   = JAMSFLD_RECVRNAME;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(to);
	jsf.Buffer = (char *)to;
	JAM_PutSubfield(jsp, &jsf);

	jsf.LoID   = JAMSFLD_SUBJECT;
	jsf.HiID   = 0;
	jsf.DatLen = strlen(subject);
	jsf.Buffer = (char *)subject;
	JAM_PutSubfield(jsp, &jsf);

	if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA) {
		jmh.Attribute |= MSG_TYPEECHO;

		if (conf.mail_conferences[confr]->fidoaddr->point) {
			sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node,
											conf.mail_conferences[confr]->fidoaddr->point);
		} else {
			sprintf(buffer, "%d:%d/%d", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node);
		}
		jsf.LoID   = JAMSFLD_OADDRESS;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);

		sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[confr]->fidoaddr->zone,
												conf.mail_conferences[confr]->fidoaddr->net,
												conf.mail_conferences[confr]->fidoaddr->node,
												conf.mail_conferences[confr]->fidoaddr->point,
												generate_msgid());

		jsf.LoID   = JAMSFLD_MSGID;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);
		jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));

	} else if (conf.mail_conferences[confr]->mail_areas[confr]->type == TYPE_NETMAIL_AREA) {
		jmh.Attribute |= MSG_TYPENET;
		jmh.Attribute |= MSG_PRIVATE;
	
		if (conf.mail_conferences[confr]->fidoaddr->point) {
			sprintf(buffer, "%d:%d/%d.%d", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node,
											conf.mail_conferences[confr]->fidoaddr->point);
		} else {
			sprintf(buffer, "%d:%d/%d", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node);
		}
		jsf.LoID   = JAMSFLD_OADDRESS;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);

		if (destaddr != NULL) {
			if (destaddr->point) {
				sprintf(buffer, "%d:%d/%d.%d", destaddr->zone,
												destaddr->net,
												destaddr->node,
												destaddr->point);
			} else {
				sprintf(buffer, "%d:%d/%d", destaddr->zone,
											destaddr->net,
											destaddr->node);
			}
			jsf.LoID   = JAMSFLD_DADDRESS;
			jsf.HiID   = 0;
			jsf.DatLen = strlen(buffer);
			jsf.Buffer = (char *)buffer;
			JAM_PutSubfield(jsp, &jsf);
		}

		sprintf(buffer, "%d:%d/%d.%d %08lx", conf.mail_conferences[confr]->fidoaddr->zone,
											conf.mail_conferences[confr]->fidoaddr->net,
											conf.mail_conferences[confr]->fidoaddr->node,
											conf.mail_conferences[confr]->fidoaddr->point,
											generate_msgid());

		jsf.LoID   = JAMSFLD_MSGID;
		jsf.HiID   = 0;
		jsf.DatLen = strlen(buffer);
		jsf.Buffer = (char *)buffer;
		JAM_PutSubfield(jsp, &jsf);
		jmh.MsgIdCRC = JAM_Crc32(buffer, strlen(buffer));
	}

	while (1) {
		z = JAM_LockMB(jb, 100);
		if (z == 0) {
			break;
		} else if (z == JAM_LOCK_FAILED) {
			sleep(1);
		} else {
			dolog("Failed to lock msg base!");
			JAM_CloseMB(jb);			
			return 1;
		}
	}
	if (JAM_AddMessage(jb, &jmh, jsp, (char *)msg, strlen(msg))) {
		dolog("Failed to add message");
		JAM_UnlockMB(jb);
		
		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
		return -1;
	} else {
		JAM_UnlockMB(jb);

		JAM_DelSubPacket(jsp);
		JAM_CloseMB(jb);
	}
	return 0;
}

void bwave_upload_reply() {
	char buffer[1024];
	char msgbuffer[1024];
	char originlinebuffer[256];
	int i;
	int bpos;
	UPL_HEADER upl_hdr;
	UPL_REC upl_rec;
	int j;
	int confr;
	int area;
	tWORD msg_attr;
	struct fido_addr addr;
	char *body;
	struct stat s;
	char *tagline;
	int echomail = 0;
	int netmail = 0;
	FILE *upl_file;
	FILE *msg_file;
	int sem_fd;
	
	snprintf(buffer, 1024, "%s/node%d", conf.bbs_path, mynode);

	if (stat(buffer, &s) != 0) {
		mkdir(buffer, 0755);
	}
	
	snprintf(buffer, 1024, "%s/node%d/", conf.bbs_path, mynode);
	
	upload_zmodem(gUser, buffer);
	
	bpos = 0;
	for (i=0;i<strlen(conf.unzip_cmd);i++) {
		if (conf.unzip_cmd[i] == '*') {
			i++;
			if (conf.unzip_cmd[i] == 'a') {
				sprintf(&buffer[bpos], "%s", upload_filename);
				bpos = strlen(buffer);
			} else if (conf.unzip_cmd[i] == 'd') {
				sprintf(&buffer[bpos], "%s/node%d/", conf.bbs_path, mynode);
				bpos = strlen(buffer);				
			} else if (conf.unzip_cmd[i] == '*') {
				buffer[bpos++] = '*';
				buffer[bpos] = '\0';
			}
		} else {
			buffer[bpos++] = conf.unzip_cmd[i];
			buffer[bpos] = '\0';
		}
	}
	system(buffer);
	
	unlink(upload_filename);
	
	snprintf(buffer, 1024, "%s/node%d/%s.UPL", conf.bbs_path, mynode, conf.bwave_name);
	
	upl_file = fopen(buffer, "r");
	
	if (!upl_file) {
		snprintf(buffer, 1024, "%s/node%d/%s.upl", conf.bbs_path, mynode, conf.bwave_name);
		upl_file = fopen(buffer, "r");
		if (!upl_file) {
			s_printf(get_string(196));
			return;
		}
	}
	
	if (!fread(&upl_hdr, sizeof(UPL_HEADER), 1, upl_file)) {
		s_printf(get_string(196));
		fclose(upl_file);
		return;		
	}
	
	confr = -1;
	area = -1;
	
	while (fread(&upl_rec, sizeof(UPL_REC), 1, upl_file)) {
		// find area
		for (i=0;i<conf.mail_conference_count;i++) {
			for (j=0;j<conf.mail_conferences[i]->mail_area_count;j++) {
				if (strcmp(conf.mail_conferences[i]->mail_areas[j]->qwkname, upl_rec.echotag) == 0) {
					confr = i;
					area = j;
					break;
				}
			}
			if (confr != -1) {
				break;
			}
		}
		
		if (confr != -1 && area != -1) {
			// import message
			if (conf.mail_conferences[confr]->mail_areas[area]->write_sec_level <= gUser->sec_level) {
				msg_attr = converts(upl_rec.msg_attr);
				
				if (msg_attr & UPL_INACTIVE) {
					continue;
				}
				
				if (strcasecmp(upl_rec.from, gUser->loginname) != 0) {
					continue;
				}
				
				addr.zone = 0;
				addr.net = 0;
				addr.node = 0;
				addr.zone = 0;				
				
				if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_NETMAIL_AREA) {
					if (!(msg_attr & UPL_NETMAIL)) {
						continue;
					}
					addr.zone = converts(upl_rec.destzone);
					addr.net = converts(upl_rec.destnet);
					addr.node = converts(upl_rec.destnode);
					addr.zone = converts(upl_rec.destpoint);
				} else if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA) {
					if (msg_attr & UPL_PRIVATE) {
						continue;
					}

				} else { // Local area
					if (msg_attr & UPL_PRIVATE) {
						continue;
					}					
				}
				
				snprintf(msgbuffer, 1024, "%s/node%d/%s", conf.bbs_path, mynode, upl_rec.filename);
				
				if (conf.mail_conferences[confr]->tagline != NULL) {
					tagline = conf.mail_conferences[confr]->tagline;
				} else {
					tagline = conf.default_tagline;
				}
				
				if (conf.mail_conferences[confr]->nettype == NETWORK_FIDO) {
					if (conf.mail_conferences[confr]->fidoaddr->point == 0) {
						snprintf(originlinebuffer, 256, "\r--- %s\r * Origin: %s (%d:%d/%d)\r", upl_hdr.reader_tear, tagline, conf.mail_conferences[confr]->fidoaddr->zone,
																								conf.mail_conferences[confr]->fidoaddr->net,
																								conf.mail_conferences[confr]->fidoaddr->node);
					} else {
					
						snprintf(originlinebuffer, 256, "\r--- %s\r * Origin: %s (%d:%d/%d.%d)\r", upl_hdr.reader_tear, tagline, conf.mail_conferences[confr]->fidoaddr->zone,
																									conf.mail_conferences[confr]->fidoaddr->net,
																									conf.mail_conferences[confr]->fidoaddr->node,
																									conf.mail_conferences[confr]->fidoaddr->point);
					}
				} else {
					snprintf(originlinebuffer, 256, "\r--- %s\r * Origin: %s \r", upl_hdr.reader_tear, tagline);
				}
			
				if (stat(msgbuffer, &s) != 0) {
					continue;
				}
				body = (char *)malloc(s.st_size + 1 + strlen(originlinebuffer));
				msg_file = fopen(msgbuffer, "r");
				if (!msg_file) {
					free(body);
					continue;
				}
				
				fread(body, 1, s.st_size, msg_file);
				fclose(msg_file);
				
				unlink(msgbuffer);
				
				body[s.st_size] = '\0';
				
				strcat(body, originlinebuffer);
				
				bpos = 0;
				for (i=0;i<strlen(body);i++) {
					if (body[i] != '\n') {
						body[bpos++] = body[i];
					}
				}
				body[bpos] = '\0';
				
				
				if (bwave_add_message(confr, area, convertl(upl_rec.unix_date), upl_rec.to, upl_rec.subj, &addr, body) != 0) {
					// failed to add message
					s_printf(get_string(197));
				} else {
					if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_NETMAIL_AREA) {
						if (conf.netmail_sem != NULL) {
							sem_fd = open(conf.netmail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
							close(sem_fd);
						}
					} else if (conf.mail_conferences[confr]->mail_areas[area]->type == TYPE_ECHOMAIL_AREA) {
						if (conf.echomail_sem != NULL) {
							sem_fd = open(conf.echomail_sem, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
							close(sem_fd);
						}						
					}
				}
				
				free(body);
			}
		}
	}
	
	fclose(upl_file);
	unlink(buffer);
	snprintf(buffer, 1024, "%s/node%d/%s.OLC", conf.bbs_path, mynode, conf.bwave_name);
	unlink(buffer);
	snprintf(buffer, 1024, "%s/node%d/%s.REQ", conf.bbs_path, mynode, conf.bwave_name);
	unlink(buffer);
			
	s_printf("\r\n");
	s_printf(get_string(6));
	s_getc();
}
